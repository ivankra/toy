#include <cstdio>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <wx/wx.h>
#include <wx/grid.h>
#include <wx/colordlg.h>
using namespace std;

class ParseError {
  string message;

public:
  ParseError(string m = "") : message(m) {}
  const string &GetMessage() { return message; }
};

// Parses and evaluates an arithmetical expression.
class Parser {
  double (*cell)(void *, const string &);
  void *cell_this;

  const char *tokptr;
  int tok;
  string tokstr;
  double toknum;

  // Parses next token
  int next() {
    while (*tokptr && isspace(*tokptr)) tokptr++;
    if (*tokptr == 0) return tok = 0;
    if (strchr("+-*/()", *tokptr) != NULL) return tok = *tokptr++;

    if (isdigit(*tokptr) || *tokptr == '.') {
      int shift = 0;
      if (sscanf(tokptr, "%lf%n", &toknum, &shift) < 1 || shift == 0)
        throw ParseError("Invalid number format");
      tokptr += shift;
      return tok = 'n';
    }

    if (isalpha(*tokptr)) {
      for (tokstr = ""; isalnum(*tokptr);)
        tokstr += *tokptr++;
      return tok = 'a';
    }

    throw ParseError(string("Unrecognized character (")+*tokptr+")");
  }

  // <factor> ::= '-' <factor> | '(' <expr> ')' | <number> | 'SQRT' '(' <expr> ')' | <name>
  double factor() {
    if (tok == '-') {
      next();
      return -factor();
    }

    if (tok == '(') {
      next();
      double res = expr();
      if (tok != ')') throw ParseError("Expected '('");
      next();
      return res;
    }

    if (tok == 'n') {
      double res = toknum;
      next();
      return res;
    }

    if (tok == 'a') {
      string s = tokstr;
      next();

      if (s == "SQRT") {
        if (tok != '(') throw ParseError("Expected '(' after SQRT");
        next();
        double x = expr();
        if (tok != ')') throw ParseError("Expected ')'");
        next();
        return sqrt(x);
      }

      if (cell == NULL) return 0.0;
      return cell(cell_this, s);
    }

    throw ParseError(string("Unexpected token (")+(char)tok+")");
  }

  // <term> ::= <factor> | <tern> '*' <factor> | <term> '/' <factor>
  double term() {
    for (double x = factor();;) {
      if (tok == '*') {
        next();
        x *= factor();
      } else if (tok == '/') {
        next();
        x /= factor();
      } else {
        return x;
      }
    }
  }

  // <expr> ::= <term> | <expr> '+' <term> | <expr> '-' <term>
  double expr() {
    for (double x = term();;) {
      if (tok == '+') {
        next();
        x += term();
      } else if (tok == '-') {
        next();
        x -= term();
      } else {
        return x;
      }
    }
  }

  bool IsNumber(const char *s) {
    double x;
    int n = 0;
    return sscanf(s, " %lf%n", &x, &n) >= 1 && n == (int)strlen(s);
  }

public:
  Parser() { cell = NULL; }

  // The user of this class must provide a function to supply
  // values of other cells in the expression.
  void SetCellFunction(double (*fn)(void *, const string &), void *cthis) {
    cell = fn;
    cell_this = cthis;
  }

  // Evaluates specified expression.
  // A ParseError exception is thrown on errors.
  double Eval(const string &s) {
    if (s == "")
      return 0;

    tokptr = s.c_str();
    if (IsNumber(tokptr))
      return atof(tokptr);

    if (*tokptr++ != '=')
      throw ParseError("Expression must begin with a '='");

    next();

    double res = expr();
    if (tok != 0) throw ParseError("Extra characters at the end of expression");
    return res;
  }
};

// Represents a spreadsheet's cell.
struct Cell {
  string text;
  int textColor, backColor;

  enum Status {
    TEXT,     // the cell contains a text data (or the formula it contains can't be evaluated)
    FORMULA,  // the cell contains a formula or a numerical value
    WAIT,     // the cell has not yet been evaluated
    CYCLIC    // the formula in the cell leads to a cyclic dependency
  };
  Status status;
  double value;

  Cell() : text(""), textColor(0x000000), backColor(0xffffff), status(TEXT), value(0.0) {}

  bool IsEmpty() const {
    return text == "" && textColor == 0 && backColor == 0xffffff;
  }
};

int ParseColor(const string &s) {
  int c = 0;
  for (int i = 0; i < 6 && i < (int)s.size(); i++) {
    int d = toupper(s[i]);
    if ('0' <= d && d <= '9') d -= '0';
    else if ('A' <= d && d <= 'F') d = d - 'A' + 10;
    else continue;
    c = c * 16 + (d & 15);
  }
  return c;
}

int ParseColor(const wxColour &c) {
  return (c.Red() << 16) | (c.Green() << 8) | c.Blue();
}

wxColour WxColor(int c) {
  return wxColour((c>>16)&0xff, (c>>8)&0xff, c&0xff);
}


// Sheet class -- represents and store speadsheet data, manages spreadsheet computations.
// Implements wxGridTableBase interface.
class Sheet : public wxGridTableBase {
private:
  vector< vector<Cell> > data;

  static const int DefaultRows = 100;
  static const int MaximumRows = 10000;

  wxGrid *view;
  wxGridCellAttrProvider *attrProv;
  bool uptodate;

public:
  // Constructs an empty spreadsheet
  Sheet() : data(DefaultRows, vector<Cell>(26)) { view = NULL; attrProv = NULL; uptodate = false; }
  virtual ~Sheet() {}

  // Saves spreadsheet to a file
  bool Save(const char *path) const {
    ofstream f(path);
    if (!f.is_open()) return false;
    for (int r = 0; r < (int)data.size(); r++) {
      assert(data[r].size() == 26);
      for (int c = 0; c < 26; c++) {
        const Cell &cell = data[r][c];
        if (cell.IsEmpty()) continue;

        char tmp[100];
        sprintf(tmp, "%.6X %.6X", cell.textColor, cell.backColor);

        f << GetCellName(r, c) << " " << tmp << " " << data[r][c].text << "\n";
      }
    }
    f.close();
    return true;
  }

  int GetNumberRows() {
    return data.size();
  }

  int GetNumberCols() {
    if (data.size() == 0) return 0;
    return data[0].size();
  }

  bool Valid(int r, int c) const {
    return 0 <= r && r < (int)data.size() && 0 <= c && c < (int)data[r].size();
  }

  bool IsEmptyCell(int row, int col) {
    return !Valid(row, col) || data[row][col].IsEmpty();
  }

  wxString GetValue(int row, int col) {
    if (!Valid(row, col)) return "";
    return data[row][col].text.c_str();
  }

  void SetValue(int row, int col, const wxString& value) {
    if (Valid(row, col) && data[row][col].text != value.c_str()) {
      data[row][col].text = value.c_str();
      data[row][col].status = Cell::WAIT;
      uptodate = false;
    }
  }

  wxString GetTypeName(int, int) {
    return "string";
  }

  bool CanGetValueAs(int, int, const wxString& typeName) {
    return typeName == "string";
  }

  bool CanSetValueAs(int, int, const wxString& typeName) {
    return typeName == "string";
  }

  long GetValueAsLong(int row, int col) {
    return Valid(row, col) ? atoi(data[row][col].text.c_str()) : 0;
  }

  double GetValueAsDouble(int row, int col) {
    return Valid(row, col) ? atof(data[row][col].text.c_str()) : 0;
  }

  bool GetValueAsBool(int, int) {
    return false;
  }

  void SetValueAsLong(int, int, double) { }
  void SetValueAsDouble(int, int, double) { }
  void SetValueAsBool(int, int, double) { }

  void* GetValueAsCustom(int, int, const wxString&) {
    return NULL;
  }

  void SetValueAsCustom(int, int, const wxString&, void*) { }

  void SetView(wxGrid* grid) {
    view = grid;
  }

  wxGrid *GetView() const {
    return view;
  }

  void Clear() {
    data = vector<vector<Cell> >(GetNumberRows(), vector<Cell>(26));
    uptodate = false;
  }

  bool InsertRows(size_t pos = 0, size_t numRows = 1) {
    for (int i = 0; i < (int)numRows && GetNumberRows() < MaximumRows; i++)
      data.insert(data.begin() + pos, vector<Cell>(26));
    uptodate = false;
    return true;
  }

  bool AppendRows(size_t numRows = 1) {
    for (int i = 0; i < (int)numRows && GetNumberRows() < MaximumRows; i++)
      data.push_back(vector<Cell>(26));
    uptodate = false;
    return true;
  }

  bool DeleteRows(size_t pos = 0, size_t numRows = 1) {
    if (pos + numRows > data.size()) {
      return false;
    } else {
      data.erase(data.begin() + pos, data.begin() + pos + numRows);
      uptodate = false;
      return true;
    }
  }

  bool InsertCols(size_t = 0, size_t = 1) { return false; }
  bool AppendCols(size_t = 1) { return false; }
  bool DeleteCols(size_t = 0, size_t = 1) { return false; }

  wxString GetRowLabelValue(int row) {
    char tmp[100];
    sprintf(tmp, "%d", row+1);
    return tmp;
  }

  wxString GetColLabelValue(int col) {
    char tmp[100];
    sprintf(tmp, "%c", 'A'+col);
    return tmp;
  }

  void SetRowLabelValue(int, const wxString&) {}
  void SetColLabelValue(int, const wxString&) {}

  void SetAttrProvider(wxGridCellAttrProvider *attrProvider) { attrProv = attrProvider; }
  wxGridCellAttrProvider* GetAttrProvider() const { return attrProv; }
  bool CanHaveAttributes() { return false; }
  void UpdateAttrRows(size_t, int) {}
  void UpdateAttrCols(size_t, int) {}
  wxGridCellAttr *GetAttr(int, int) { return NULL; }
  void SetAttr(wxGridCellAttr *, int, int) {}
  void SetRowAttr(wxGridCellAttr*, int) {}
  void SetColAttr(wxGridCellAttr*, int) {}

  // Returns specified cell
  const Cell &GetCell(int r, int c) {
    if (!Valid(r, c))
      throw "Invalid cell address";
    return data[r][c];
  }

  void SetCellColors(int r, int c, int text = -1, int back = -1) {
    if (Valid(r, c)) {
      if (text != -1) data[r][c].textColor = text;
      if (back != -1) data[r][c].backColor = back;
    }
  }

  string GetCellName(int r, int c) const {
    if (!Valid(r, c))
      return "";

    char buf[100];
    sprintf(buf, "%c%d", c+'A', r+1);
    return string(buf);
  }

  bool ParseCell(const char *name, int &r, int &c) const {
    if (name == NULL || name[0] == 0 || !isalpha(name[0])) return false;

    c = toupper(name[0]) - 'A';
    if (c < 0 || c >= 26) return false;

    if (sscanf(name+1, "%d", &r) != 1) return false;
    r--;
    if (r < 0 || r >= MaximumRows) return false;

    return true;
  }

  // Recomputes spreadsheet
  bool Compute() {
    if (uptodate) return false;

    for (int r = 0; r < (int)data.size(); r++)
      for (int c = 0; c < (int)data[r].size(); c++)
        data[r][c].status = Cell::WAIT;

    for (int r = 0; r < (int)data.size(); r++)
      for (int c = 0; c < (int)data[r].size(); c++)
        if (data[r][c].status == Cell::WAIT)
          ComputeCell(r, c);

    uptodate = true;
    return true;
  }

private:
  static double CellFn(void *p, const string &s) {
    Sheet *sh = (Sheet *)p;

    int r1, c1;
    if (!sh->ParseCell(s.c_str(), r1, c1))
      throw ParseError("Invalid cell address");

    sh->ComputeCell(r1, c1);
    if (sh->data[r1][c1].status == Cell::CYCLIC)
      throw ParseError("CYCLE");

    return sh->data[r1][c1].value;
  }

  // Recursively computes value of a cell (r,c), detecting cyclic dependencies
  void ComputeCell(int r, int c) {
    Cell &cell = data[r][c];
    if (cell.status != Cell::WAIT) return;

    cell.status = Cell::CYCLIC;
    cell.value = 0.0;

    if (cell.text == "") {
      cell.status = Cell::TEXT;
      return;
    }

    try {
      Parser p;
      p.SetCellFunction(&Sheet::CellFn, this);
      double res = p.Eval(cell.text);
      cell.status = Cell::FORMULA;
      cell.value = res;
    } catch (ParseError &e) {
      cell.status = (e.GetMessage() == "CYCLE") ? Cell::CYCLIC : Cell::TEXT;
    }
  }
};

class AboutWindow : public wxDialog {
  static const int Width = 400;
  static const int Height = 200;

  void OnClose(wxEvent &) { Destroy(); }
public:
  AboutWindow() : wxDialog(NULL, -1, "About", wxPoint(-1, -1), wxSize(Width, Height))
  {
    const char *about =
      "Spreadsheet v0.1\n"
      "Features:\n"
      "- Supported arithmetic operations: +, -, *, /, (), SQRT\n"
      "- Excel-style formulas (e.g. =A1+B2)\n"
      "- Cell formatting: can specify background/text color for each cell\n"
      "- Saves spreadsheets to/loads from files\n";
    new wxStaticText(this, -1, about, wxPoint(8, 8), wxSize(Width-16, Height-16));

    Connect(GetId(), wxEVT_CLOSE_WINDOW, wxObjectEventFunction(&AboutWindow::OnClose));
  }
};

// A basic spreadsheet view and editing window
class SheetWindow : public wxFrame {
private:
  static const int MenuFileNew = 10001;
  static const int MenuFileOpen = 10002;
  static const int MenuFileSave = 10003;
  static const int MenuFileClose = 10004;
  static const int MenuModeViewText = 10005;
  static const int MenuModeViewRes = 10006;
  static const int MenuFormatTextColor = 10007;
  static const int MenuFormatBackColor = 10008;
  static const int MenuAbout = 10009;

  enum Mode { ViewText, ViewResults };
  Mode mode;

  Sheet *ss;
  wxGrid *grid;

  class MyCellRenderer : public wxGridCellStringRenderer {
    SheetWindow *window;
  public:
    MyCellRenderer(SheetWindow *w) : window(w) {}
    ~MyCellRenderer() {}

    wxGridCellRenderer *Clone() const { return new MyCellRenderer(window); }

    void Draw(wxGrid &grid, wxGridCellAttr &attr, wxDC &dc, const wxRect &rect, int row, int col, bool isSelected) {
      const Cell &cell = window->ss->GetCell(row, col);
      string s = cell.text;

      if (window->mode == ViewResults) {
        window->ss->Compute();

        if (cell.status == Cell::FORMULA) {
          char tmp[100];
          snprintf(tmp, sizeof(tmp), "%.2f", cell.value);
          s = tmp;
        }
      }

      if (isSelected) {
        dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
        dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
      } else {
        dc.SetTextForeground(WxColor(cell.textColor));
        dc.SetBrush(wxBrush(WxColor(cell.backColor)));
      }
      dc.SetPen(wxPen(wxColour(0,0,0)));

      dc.SetFont(attr.GetFont());
      dc.DrawRectangle(rect.GetX()-1, rect.GetY()-1, rect.GetWidth()+2, rect.GetHeight()+2);

      dc.DrawText(s.c_str(), rect.GetX(), rect.GetY());
    }
  };

  void UpdateView() {
    string title = "Spreadsheet";
    SetTitle(title.c_str());

    grid->ForceRefresh();
  }

  void OnMenuNew(wxEvent &) {
    SheetWindow *w = new SheetWindow();
    w->Show(true);
  }

  void OnMenuSave(wxEvent &) {
    string s = std::string(wxFileSelector("Choose file to save to", "", "", "data").mb_str());
    if (s != "" && !ss->Save(s.c_str()))
      wxMessageBox("Failed to save the current spreadsheet to the specified file", "Error");
  }

  void OnMenuOpen(wxEvent &) {
    string path = std::string(wxFileSelector("Choose file to open", "", "", "data").mb_str());
    if (path != "") {
      DoOpen(path);
    }
  }

  void DoOpen(string path) {
    ifstream f(path.c_str());
    if (!f.is_open()) {
      wxMessageBox("Failed to open specified file", "Error");
      return;
    }

    grid->BeginBatch();
    grid->ClearGrid();

    string line;
    while (getline(f, line)) {
      istringstream is(line);
      string name, c1, c2;
      is >> name >> c1 >> c2;
      size_t pos = is.tellg();
      if (pos < line.size() && isspace(line[pos])) pos++;
      string text = line.substr(pos);

      int row, col;
      if (!ss->ParseCell(name.c_str(), row, col))
        goto fail;

      if (grid->GetNumberRows() < row+1)
        grid->AppendRows(row+1 - grid->GetNumberRows());

      grid->SetCellValue(row, col, text.c_str());
      ss->SetCellColors(row, col, ParseColor(c1), ParseColor(c2));
    }

    f.close();
    grid->EndBatch();
    UpdateView();
    return;

fail:
    f.close();
    grid->ClearGrid();
    grid->EndBatch();
    UpdateView();
    wxMessageBox("Spreadsheet file is invalid or corrupted\n", "Error");
  }

  void OnMenuViewText(wxEvent &) { mode = ViewText; UpdateView(); }
  void OnMenuViewResults(wxEvent &) { mode = ViewResults; UpdateView(); }
  void OnClose(wxEvent &) { Destroy(); }
  void OnResize(wxEvent &) { grid->SetSize(GetClientSize()); }
  void OnGridChange(wxEvent &) { /*if (ss->Compute()) UpdateView();*/ }
  void OnAbout(wxEvent &) { (new AboutWindow())->Show(true); }

  void OnMenuColor(wxEvent &e) {
    int row = grid->GetGridCursorRow();
    int col = grid->GetGridCursorCol();
    if (!ss->Valid(row, col)) return;

    wxColour c = wxGetColourFromUser();
    if (!c.Ok()) return;

    if (e.GetId() == MenuFormatBackColor)
      ss->SetCellColors(row, col, -1, ParseColor(c));
    else
      ss->SetCellColors(row, col, ParseColor(c), -1);
    UpdateView();
  }

  void MakeMenu() {
    wxMenuBar *bar = new wxMenuBar();

    wxMenu *m = new wxMenu();
    m->Append(MenuFileNew, "&New");
    m->Append(MenuFileOpen, "&Open");
    m->Append(MenuFileSave, "&Save as");
    m->Append(MenuFileClose, "&Close");
    bar->Append(m, "&File");

    Connect(MenuFileNew, wxEVT_COMMAND_MENU_SELECTED, wxObjectEventFunction(&SheetWindow::OnMenuNew));
    Connect(MenuFileOpen, wxEVT_COMMAND_MENU_SELECTED, wxObjectEventFunction(&SheetWindow::OnMenuOpen));
    Connect(MenuFileSave, wxEVT_COMMAND_MENU_SELECTED, wxObjectEventFunction(&SheetWindow::OnMenuSave));
    Connect(MenuFileClose, wxEVT_COMMAND_MENU_SELECTED, wxObjectEventFunction(&SheetWindow::OnClose));

    m = new wxMenu();
    m->Append(MenuModeViewText, "View &text/formulas");
    m->Append(MenuModeViewRes, "View &results");
    bar->Append(m, "&Mode");

    Connect(MenuModeViewText, wxEVT_COMMAND_MENU_SELECTED, wxObjectEventFunction(&SheetWindow::OnMenuViewText));
    Connect(MenuModeViewRes, wxEVT_COMMAND_MENU_SELECTED, wxObjectEventFunction(&SheetWindow::OnMenuViewResults));

    m = new wxMenu();
    m->Append(MenuFormatTextColor, "&Text color");
    m->Append(MenuFormatBackColor, "&Background color");
    bar->Append(m, "&Format");

    Connect(MenuFormatTextColor, wxEVT_COMMAND_MENU_SELECTED, wxObjectEventFunction(&SheetWindow::OnMenuColor));
    Connect(MenuFormatBackColor, wxEVT_COMMAND_MENU_SELECTED, wxObjectEventFunction(&SheetWindow::OnMenuColor));

    m = new wxMenu();
    m->Append(MenuAbout, "&About");
    bar->Append(m, "&Help");

    Connect(MenuAbout, wxEVT_COMMAND_MENU_SELECTED, wxObjectEventFunction(&SheetWindow::OnAbout));

    this->SetMenuBar(bar);
  }

public:
  SheetWindow() : wxFrame(NULL, -1, "", wxPoint(-1, -1), wxSize(640, 480)) {
    ss = new Sheet();
    mode = ViewResults;

    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    MakeMenu();

    grid = new wxGrid(this, -1, wxPoint(0, 0), GetClientSize());
    grid->SetDefaultRenderer(new MyCellRenderer(this));
    grid->SetTable(ss, false);
    Connect(grid->GetId(), wxEVT_GRID_CELL_CHANGE, wxObjectEventFunction(&SheetWindow::OnGridChange));

    Connect(GetId(), wxEVT_CLOSE_WINDOW, wxObjectEventFunction(&SheetWindow::OnClose));
    Connect(GetId(), wxEVT_SIZE, wxObjectEventFunction(&SheetWindow::OnResize));

    UpdateView();
  }

  ~SheetWindow() { delete ss; }

  void Open(string path) { DoOpen(path); }
};

class SpreadsheetApp : public wxApp {
public:
  virtual bool OnInit() {
    SheetWindow *w = new SheetWindow();
    SetTopWindow(w);
    w->Show(true);
    if (argc >= 2) {
      w->Open(string(argv[1].mb_str()));
    }
    return true;
  }
};

IMPLEMENT_APP(SpreadsheetApp)
