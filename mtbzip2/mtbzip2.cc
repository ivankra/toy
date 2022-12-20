// mtbzip2: параллельная версия программы сжатия bzip2.
//
// Программа вызывается с командной строки и по умолчанию читает данные
// со стандартного входа, и пишет сжатый файл на стандартный выход.
//
// Поддерживаемые флаги, которые могут быть указаны в командной строке:
//  -1 .. -9   выбор размера блока для метода bzip2 (100Кб..900Кб)
//  -p <n>     задает число параллельных потоков, испольщующихся для сжатия
//             на локальной машине
//  -k         не удалять входные файлы после сжатия
//
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cctype>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <queue>
#include <string>
using namespace std;

#ifdef MPIBZIP2
#include <mpi.h>
#endif

extern "C" {
    #include "bzlib_private.h"
    void BZ2_compressBlock(EState* s, Bool is_last_block);
};

// Базовый класс объектов, представляющих потоки выполнения
class Runnable {
  public:
    virtual void Run() = 0;
    virtual ~Runnable() {};
};

static void *StartThreadCallback(void *arg) {
    Runnable *runnable = (Runnable *)arg;
    runnable->Run();
    return NULL;
}

// Запускает поток на выполнение и возвращает его pthreads-идентификатор
pthread_t StartThread(Runnable *runnable) {
    pthread_t id;
    pthread_create(&id, NULL, StartThreadCallback, (void *)runnable);
    return id;
}

void die(const char *msg = NULL) {
    if (msg != NULL) fprintf(stderr, "Fatal error: %s", msg);
#ifdef MPIBZIP2
    MPI_Finalize();
#endif
    exit(1);
}

unsigned char *xmalloc(uint32_t n) {
    void *p = malloc(n);
    if (p == NULL) die("Out of memory\n");
    return (unsigned char *)p;
}

// упаковка 32-битного целого в память
void pack32(unsigned char *p, uint32_t x) {
    for (int i = 0; i < 4; i++) { *p++ = x & 0xff; x >>= 8; }
}

// распаковка 32-битного целого
uint32_t unpack32(unsigned char *p) {
    uint32_t x = 0;
    for (int i = 3; i >= 0; i--) { x <<= 8; x |= p[i]; }
    return x;
}

// Определяет число доступных процессорных ядер в системе
int DetectCPUs() {
#ifdef _SC_NPROCESSORS_ONLN
    int n = sysconf(_SC_NPROCESSORS_ONLN);     // для Linux
    if (n >= 1) return n;
#endif
    char *s = getenv("NUMBER_OF_PROCESSORS");  // для Windows
    if (s != NULL && isdigit(s[0])) return max(1, atoi(s));
    return 1;
}

// Класс BzipBlockCompressor. Взаимодействует с библиотекой bzip2 1.0.4/1.0.5,
// предоставляет интерфейс для сжатия отдельного блока данных.
class BzipBlockCompressor {
  public:
    BzipBlockCompressor(int blockSize100k);
    ~BzipBlockCompressor();

    // Процедура для сжатия одного блока.
    // size: размер входного блока в байтах
    // crc: CRC-сумма исходных данных блока (до применения RLE-сжатия)
    void Compress(uint32_t input_size, uint32_t crc);

    // Возвращает указатель на буфер для входных данных
    unsigned char *InputBuffer() const { return s.block; }

    // Возвращает указатель на буфер с выходными данными и их размер в битах
    const unsigned char *OutputBuffer() const { return s.zbits; }
    uint32_t OutputBits() const { return s.numZ * 8 + s.bsLive - 80; }

  private:
    EState s;
    BzipBlockCompressor(const BzipBlockCompressor &) {};
    void operator =(const BzipBlockCompressor &) {};
};

BzipBlockCompressor::BzipBlockCompressor(int blockSize100k) {
    uint32_t n = 100000 * blockSize100k;
    memset(&s, 0, sizeof(EState));
    s.arr1 = (UInt32 *)xmalloc(n * sizeof(UInt32));
    s.arr2 = (UInt32 *)xmalloc((n+BZ_N_OVERSHOOT) * sizeof(UInt32));
    s.ftab = (UInt32 *)xmalloc(65537 * sizeof(UInt32));
    s.blockSize100k = blockSize100k;
    s.nblockMAX = 100000 * blockSize100k - 19;
    s.workFactor = 30;
    s.block = (UChar*)s.arr2;
    s.mtfv = (UInt16*)s.arr1;
    s.ptr = (UInt32*)s.arr1;
}

void BzipBlockCompressor::Compress(uint32_t input_size, uint32_t crc) {
    s.numZ = s.bsLive = s.bsBuff = s.combinedCRC = 0;
    s.blockNo = 2;
    s.blockCRC = crc ^ 0xffffffffL;
    s.nblock = input_size;
    memset(s.inUse, 0, sizeof(s.inUse));
    for (unsigned char *p = s.block, *q = p + input_size; p < q;)
        s.inUse[*p++] = 1;
    BZ2_compressBlock(&s, 1);
}

BzipBlockCompressor::~BzipBlockCompressor() {
    free(s.arr1); free(s.arr2); free(s.ftab);
}

// Класс BitStreamWriter
// Оборачивает объект типа FILE*, позволяя записывать в него
// данные блоками с произвольным числом двоичных битов.
class BitStreamWriter {
  public:
    BitStreamWriter(FILE *fp, int bufferSize);
    ~BitStreamWriter();
    void Write(const unsigned char *data, uint32_t bits);
    void Flush();

  private:
    FILE *fp;
    unsigned char *buffer, *tail, *bufend;
    uint32_t reg, live;

    BitStreamWriter(const BitStreamWriter &) {}
    void operator =(const BitStreamWriter &) {}
};

BitStreamWriter::BitStreamWriter(FILE *fp, int bufferSize) {
    this->fp = fp;
    tail = buffer = xmalloc(bufferSize);
    bufend = buffer + bufferSize;
    live = reg = 0;
}

BitStreamWriter::~BitStreamWriter() {
    if (live > 0) Write((const unsigned char *)"\0", 8 - live);
    Flush();
    free(buffer);
    fclose(fp);
}

void BitStreamWriter::Write(const unsigned char *data, uint32_t bits) {
    uint32_t r = reg;
    for (; bits >= 8; bits -= 8) {
        r <<= 8;
        r |= ((uint32_t)(*data++)) << (8 - live);
        *tail = (r >> 8) & 0xffL;
        if (++tail == bufend) Flush();
    }
    if (bits != 0) {
        r <<= 8;
        r |= ((uint32_t)(*data++)) << (8 - live);
        live += bits;
        if (live >= 8) {
            live -= 8;
            *tail = (r >> 8) & 0xffL;
            if (++tail == bufend) Flush();
        } else {
            r >>= 8;
        }
        r >>= 8 - live; r <<= 8 - live;
    }
    reg = r;
}

void BitStreamWriter::Flush() {
    size_t n = tail - buffer;
    if (n != 0) {
        size_t m = fwrite(buffer, 1, n, fp);
        if (n != m) die("Failed to write data to output file\n");
        tail = buffer;
    }
}

// Класс OutputThread
// Представляет собой поток, который получает от рабочих потоков
// сжатые блоки, упорядочивает их по номеру и записывает в выходной файл.
class OutputThread : public Runnable {
    BitStreamWriter *writer;
    uint64_t next_id, last_id;
    pthread_mutex_t mutex;
    pthread_cond_t condvar;

    struct Rec { unsigned char *data; uint32_t bits, crc; };
    map<uint64_t, Rec> completed;

  public:
    OutputThread(BitStreamWriter *writer, int blockSize100k)  {
        unsigned char magic[4] = { 'B', 'Z', 'h', '0' + blockSize100k };
        writer->Write(magic, 32);  // запись заголовка bz2-файла
        this->writer = writer;
        next_id = 1;
        last_id = (uint64_t)(-1);
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&condvar, NULL);
    }

    ~OutputThread() {
        pthread_cond_destroy(&condvar);
        pthread_mutex_destroy(&mutex);
        delete writer;
    }

    virtual void Run() {
        uint32_t c_crc = 0;
        pthread_mutex_lock(&mutex);
        while (true) {
            if (next_id > last_id) break;
            if (completed.size() == 0 || completed.begin()->first != next_id) {
                pthread_cond_wait(&condvar, &mutex);
                continue;
            }

            Rec rec = completed.begin()->second;
            completed.erase(next_id++);

            pthread_mutex_unlock(&mutex);
            writer->Write(rec.data, rec.bits);
            free(rec.data);
            c_crc = ((c_crc << 1) | (c_crc >> 31)) ^ rec.crc;
            pthread_mutex_lock(&mutex);
        }
        pthread_mutex_unlock(&mutex);

        // запись маркера конца файла и CRC-суммы всего входного файла
        unsigned char a[10] = {
            0x17, 0x72, 0x45, 0x38, 0x50, 0x90, (c_crc >> 24) & 0xff,
            (c_crc >> 16) & 0xff, (c_crc >> 8) & 0xff, c_crc & 0xff
        };
        writer->Write(a, 80);

        delete writer;
        writer = NULL;
    }

    void Add(uint64_t block_id, unsigned char *data, uint32_t bits, uint32_t crc) {
        pthread_mutex_lock(&mutex);
        Rec rec = { data, bits, crc };
        completed[block_id] = rec;
        pthread_cond_signal(&condvar);
        pthread_mutex_unlock(&mutex);
    }

    void SetLastBlock(uint64_t id) {
        pthread_mutex_lock(&mutex);
        last_id = id;
        pthread_cond_signal(&condvar);
        pthread_mutex_unlock(&mutex);
    }
};

struct InputBlock {
    unsigned char *data;
    uint32_t size, crc;
    uint64_t id;
};

// Класс InputThread
// Представляет собой поток, осуществляющий чтение входного файла,
// сжатие его методом RLE и разбиением на блоки.
class InputThread : public Runnable {
  public:
    InputThread(FILE *fp, int blockSize100k, int bufferSize, int queueSize);
    ~InputThread();
    virtual void Run();
    uint64_t GetBlocksCount() const { return block_id; }
    InputBlock *Get();
    void Put(InputBlock *b);

  private:
    FILE *fp;
    uint32_t rle_ch, rle_len, crc, nblock, nblockMAX, bufferSize;
    uint64_t block_id;
    unsigned char *block, *buffer;
    pthread_mutex_t mutex;
    pthread_cond_t free_cv, busy_cv;
    vector<InputBlock *> free_queue;
    queue<InputBlock *> busy_queue;
    InputBlock *blk;

    void PrepareBlock();
    void DispatchBlock();

    // вспомогательная процедура для RLE-сжатия
    inline void add_pair() {
        switch (rle_len) {
          case 1:
            block[nblock++] = (unsigned char)rle_ch;
            break;
          case 2:
            block[nblock++] = (unsigned char)rle_ch;
            block[nblock++] = (unsigned char)rle_ch;
            break;
          case 3:
            block[nblock++] = (unsigned char)rle_ch;
            block[nblock++] = (unsigned char)rle_ch;
            block[nblock++] = (unsigned char)rle_ch;
            break;
          default:
            block[nblock++] = (unsigned char)rle_ch;
            block[nblock++] = (unsigned char)rle_ch;
            block[nblock++] = (unsigned char)rle_ch;
            block[nblock++] = (unsigned char)rle_ch;
            block[nblock++] = (unsigned char)(rle_len - 4);
            break;
        }
        while (rle_len-- != 0) BZ_UPDATE_CRC(crc, rle_ch);
    }

    InputThread(const InputThread &) : Runnable() {}
    void operator =(const InputThread &) {}
};

InputThread::InputThread(FILE *fp, int blockSize100k, int bufferSize, int queueSize) {
    this->fp = fp;
    this->bufferSize = bufferSize;
    buffer = xmalloc(bufferSize);
    nblockMAX = 100000 * blockSize100k - 19;
    block_id = 0;
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&free_cv, NULL);
    pthread_cond_init(&busy_cv, NULL);
    for (int i = 0; i < queueSize; i++) {
        free_queue.push_back(new InputBlock());
        free_queue.back()->data = xmalloc(100000 * blockSize100k);
    }
}

InputThread::~InputThread() {
    pthread_cond_destroy(&busy_cv);
    pthread_cond_destroy(&free_cv);
    pthread_mutex_destroy(&mutex);
    free(buffer);
    for (size_t i = 0; i < free_queue.size(); i++) {
        free(free_queue[i]->data);
        delete free_queue[i];
    }
}

// Главный цикл, осуществляющий чтение и RLE-сжатие входного файла
void InputThread::Run() {
    unsigned char *ptr = NULL;
    uint32_t avail = 0, ch;

    nblock = nblockMAX;
    block = NULL;
    rle_ch = 256;
    rle_len = 0;

    while (true) {
        if (avail == 0) {
            avail = fread(buffer, 1, bufferSize, fp);
            if (avail == 0) break;
            ptr = buffer;
        }

        if (nblock >= nblockMAX) {
            if (block != NULL) {
                BZ_FINALISE_CRC(crc);
                DispatchBlock();
            }
            PrepareBlock();
            nblock = 0;
            BZ_INITIALISE_CRC(crc);
        }

        ch = *ptr++;
        avail--;

        if (ch != rle_ch && rle_len == 1) {
            BZ_UPDATE_CRC(crc, rle_ch);
            block[nblock++] = rle_ch;
            rle_ch = ch;
        } else if (ch == rle_ch && rle_len != 255) {
            ++rle_len;
        } else {
            if (rle_ch != 256) add_pair();
            rle_ch = ch;
            rle_len = 1;
        }
    }

    if (ferror(fp)) {
        perror("fread");
        die("Failed to read data from input file\n");
    }

    if (rle_ch != 256 && rle_len > 0) add_pair();

    if (block != NULL && nblock != 0) {
        BZ_FINALISE_CRC(crc);
        DispatchBlock();
    }

    fclose(fp);
    fp = NULL;

    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&busy_cv);
    pthread_mutex_unlock(&mutex);
}

void InputThread::PrepareBlock() {
    pthread_mutex_lock(&mutex);
    while (free_queue.size() == 0)
        pthread_cond_wait(&free_cv, &mutex);
    blk = free_queue.back();
    free_queue.pop_back();
    pthread_mutex_unlock(&mutex);

    block = blk->data;
    block_id++;
}

void InputThread::DispatchBlock() {
    blk->size = nblock;
    blk->crc = crc;
    blk->id = block_id;
    pthread_mutex_lock(&mutex);
    busy_queue.push(blk);
    pthread_cond_signal(&busy_cv);
    pthread_mutex_unlock(&mutex);
}

// Эта процедура вызывается рабочими потоками для получения очередного блока
// для сжатия. Если требуется, процедура блокирует выполнения потока пока
// очередной блок не будет прочтён. При достижении конца файла возвращает NULL.
InputBlock *InputThread::Get() {
    InputBlock *b = NULL;
    pthread_mutex_lock(&mutex);
    while (busy_queue.size() == 0 && fp != NULL)
        pthread_cond_wait(&busy_cv, &mutex);
    if (busy_queue.size() != 0) {
        b = busy_queue.front();
        busy_queue.pop();
    }
    pthread_mutex_unlock(&mutex);
    return b;
}

// Вызывается рабочими потоками, чтобы "вернуть" блок, ранее
// полученный ими от процедуры Get()
void InputThread::Put(InputBlock *b) {
    pthread_mutex_lock(&mutex);
    free_queue.push_back(b);
    pthread_cond_signal(&free_cv);
    pthread_mutex_unlock(&mutex);
}

// Класс WorkerThread
// Представляет собой рабочий поток, в цикле получающий блоки для сжатия от
// InputThread, сжимающий их с использованием BzipBlockCompressor и передающий
// результаты работы в OutputThread для записи в выходной файл.
class WorkerThread : public Runnable {
    BzipBlockCompressor *compressor;
    InputThread *ithread;
    OutputThread *othread;

  public:
    WorkerThread(int blockSize100k, InputThread *ithread, OutputThread *othread) {
        this->ithread = ithread;
        this->othread = othread;
        compressor = new BzipBlockCompressor(blockSize100k);
    }
    ~WorkerThread() { delete compressor; }

    virtual void Run() {
        InputBlock *blk;
        while ((blk = ithread->Get()) != NULL) {
            uint32_t size = blk->size, crc = blk->crc;
            uint64_t id = blk->id;
            memcpy(compressor->InputBuffer(), blk->data, size);
            ithread->Put(blk);

            compressor->Compress(size, crc);

            uint32_t bits = compressor->OutputBits();
            unsigned char *p = xmalloc((bits + 7) / 8);
            memcpy(p, compressor->OutputBuffer(), (bits + 7) / 8);
            othread->Add(id, p, bits, crc);
        }
    }
};

#ifdef MPIBZIP2
enum { TAG_INIT = 1, TAG_WORK = 2, TAG_RESULTS = 3, TAG_FINISH = 4 };

// Главный цикл каждого MPI-процесса, не являющегося мастером
// (с не-нулевым рангом)
void mpi_slave(MPI_Comm comm, int blockSize100k)
{
    BzipBlockCompressor compressor(blockSize100k);
    unsigned char *buf = NULL;
    int err, len = 0, tag = TAG_INIT;
    MPI_Status status;

    while (true) {
        // Отправка процессу-мастеру сообщения с результатами работы -
        // сжатым блоком данных (или сообщения TAG_INIT в первый раз),
        // и приём очередного сообщения в ответ.
        err = MPI_Sendrecv(
            buf, len, MPI_BYTE, 0, tag,
            compressor.InputBuffer(), blockSize100k*100000,
            MPI_BYTE, 0, MPI_ANY_TAG, comm, &status);
        assert(err == 0);

        if (status.MPI_TAG == TAG_FINISH) {
            // получили сообщение о том, что блоков больше нет, выходим
            break;
        } else if (status.MPI_TAG == TAG_WORK) {
            // сообщение содержит очередной блок, который нужно сжать
            MPI_Get_elements(&status, MPI_BYTE, &len);
            buf = compressor.InputBuffer();
            compressor.Compress(len - 4, unpack32(buf + len - 4));

            // подготовка сообщения-ответа
            buf = (unsigned char *)compressor.OutputBuffer();
            len = (compressor.OutputBits() + 7) / 8 + 4;
            pack32(buf + len - 4, compressor.OutputBits());
            tag = TAG_RESULTS;
        }
    }
}

// Главный цикл MPI программы-мастера (ранга 0), общающегося c удалёнными
// процессами.
void mpi_master(MPI_Comm comm, InputThread *ithread, OutputThread *othread) {
    InputBlock *b, *next_block = NULL;
    unsigned char *buffer, small_buf[10];
    int from, len, mpisize;
    MPI_Status status;
    MPI_Comm_size(comm, &mpisize);
    if (mpisize == 1) return;

    // slaves[i] = текущий блок, обрабатываемый процессом ранга i
    vector<InputBlock *> slaves(mpisize);
    int in_flight = 0;  // общее число блоков, обрабатываемых сейчас удаленно
    vector<MPI_Request> req(mpisize);

    while (true) {
        // получение очередного входного блока, если нужно, и
        // проверка условия остановки.
        if (next_block == NULL) {
            next_block = ithread->Get();
            if (next_block == NULL && in_flight == 0) break;
        }

        // получение длины очередного сообщения
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &status);
        MPI_Get_elements(&status, MPI_BYTE, &len);

        // приём сообщения от удаленного процесса
        buffer = (status.MPI_TAG == TAG_RESULTS ? xmalloc(len) : small_buf);
        MPI_Recv(buffer, len, MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG,
                comm, &status);
        from = status.MPI_SOURCE;
        MPI_Get_elements(&status, MPI_BYTE, &len);

        // если в сообщении содержится готовый сжатый блок,
        // передаём его в поток OutputThread
        if (status.MPI_TAG == TAG_RESULTS) {
            MPI_Wait(&req[from], &status);
            b = slaves[from];  slaves[from] = NULL;
            assert(b != NULL && len >= 4);
            othread->Add(b->id, buffer, unpack32(buffer + len - 4), b->crc);
            ithread->Put(b);
            in_flight--;
        }

        // отправляем сообщение с очередным блоком, который нужно сжать
        if (next_block != NULL) {
            b = next_block;  next_block = NULL;  slaves[from] = b;
            pack32(b->data + b->size, b->crc);
            in_flight++;
            MPI_Isend(b->data,b->size+4,MPI_BYTE,from,TAG_WORK,comm,&req[from]);
        }
    }

    // сообщаем всем удаленным процессам, что блоков больше не будет
    for (int i = 1; i < mpisize; i++)
        MPI_Send(small_buf, 0, MPI_BYTE, i, TAG_FINISH, comm);
}
#endif

// Процедура для сжатия отдельного файла.
// fin, fout: открытый входной и выходной файлы
// blockSize100k: размер bzip2-блока (от 1 до 9)
// numLocalWorkers: число локальных параллельных потоков для сжатия
void Compress(FILE *fin, FILE *fout, int blockSize100k, int numLocalWorkers) {
    const int kInBuf = 1048576, kOutBuf = 1048576;
    int mpisize = 0;
#ifdef MPIBZIP2
    MPI_Comm_size(MPI_COMM_WORLD, &mpisize);
#endif

    InputThread ithread(fin, blockSize100k, kInBuf, numLocalWorkers+mpisize+2);
    OutputThread othread(new BitStreamWriter(fout, kOutBuf), blockSize100k);

    // запуск потоков ввода/вывода на выполнение
    pthread_t ithread_handle = StartThread(&ithread);
    pthread_t othread_handle = StartThread(&othread);

    // запуск локальных потоков
    vector<WorkerThread *> workers;
    for (int i = 0; i < numLocalWorkers; i++) {
        workers.push_back(new WorkerThread(blockSize100k, &ithread, &othread));
        pthread_detach(StartThread(workers.back()));
    }

#ifdef MPIBZIP2
    mpi_master(MPI_COMM_WORLD, &ithread, &othread);
#endif

    // ожидаем, пока входной файл не будет полностью прочтён
    pthread_join(ithread_handle, NULL);

    // передача общего числа блоков в объект OutputThread, чтобы он
    // знал когда нужно остановиться, и ожидаем завершения его работы
    othread.SetLastBlock(ithread.GetBlocksCount());
    pthread_join(othread_handle, NULL);

    for (size_t i = 0; i < workers.size(); i++) delete workers[i];
}

// Точка входа в программу
int main(int argc, char **argv) {
    int blockSize100k = 9, numLocalWorkers = DetectCPUs(), keepFlag = 0;
    vector<string> files;

#ifdef MPIBZIP2
    // Инициализация MPI, получение ранга текущего процесса
    int rank = 0;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    numLocalWorkers = 1;
#endif

    // разбор параметров командной строки
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            files.push_back(string(argv[i]));
        } else if (strlen(argv[i]) == 2 && isdigit(argv[i][1])) {
            blockSize100k = argv[i][1] - '0';
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            numLocalWorkers = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-k") == 0) {
            keepFlag = 1;
        } else {
            // вывод справки о параметрах командной строки
            fprintf(stderr, "Usage: %s [flags] [input files]\n"
              "  -1 .. -9     set block size to 100k .. 900k\n"
              "  -p <n>       use n parallel threads on local machine\n"
              "  -k           keep (don't delete) input files\n"
              "If no files are given, compression is from stdin to stdout\n",
              argv[0]);
            die();
        }
    }

#ifdef MPIBZIP2
    if (rank != 0)
        mpi_slave(MPI_COMM_WORLD, blockSize100k);
    else
#endif
    {
        if (files.size() == 0) {
            Compress(stdin, stdout, blockSize100k, numLocalWorkers);
        } else {
            for (size_t i = 0; i < files.size(); i++) {
                string s = files[i], t = s + ".bz2";
                FILE *f = fopen(s.c_str(), "rb");
                if (f == NULL) { perror("fopen"); die("Can't open input file\n"); }
                FILE *g = fopen(t.c_str(), "wb");
                if (g == NULL) {perror("fopen");die("Can't create output file\n");}
                Compress(f, g, blockSize100k, numLocalWorkers);
                if (!keepFlag) unlink(s.c_str());
            }
        }
    }

#ifdef MPIBZIP2
    MPI_Finalize();
#endif
    return 0;
}
