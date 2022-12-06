#!/usr/bin/python
# A simple implementation of the game of Tetris in python.
#
# Controls:
#   * arrow keys to move pieces
#   * Enter/space/tab to rotate
#   * p to pause
#
# Author: Ivan Krasilnikov, 2011

import sys, time, random, pygame

piece_color = {
    None: (0, 0, 0),
    'I': (0, 255, 255),
    'J': (64, 64, 255),
    'L': (255, 160, 0),
    'O': (255, 255, 0),
    'S': (64, 204, 64),
    'T': (255, 64, 255),
    'Z': (255, 64, 64)
}

def make_dark_color(rgb):
    r, g, b = rgb
    q = 75
    return (r*q//100, g*q//100, b*q//100)


class Field:
    def __init__(self, height, width):
        self.height = height
        self.width = width
        self.data = [None] * (width * height)

    def __getitem__(self, rc):
        r, c = rc
        return self.data[r * self.width + c]

    def __setitem__(self, rc, value):
        r, c = rc
        self.data[r * self.width + c] = value

    def cells(self, y0 = 0, x0 = 0):
        res = set()
        for y in range(self.height):
            for x in range(self.width):
                if self[y, x]:
                    res.add((y0 + y, x0 + x))
        return res

    def contains(self, r, c):
        return (0 <= r < self.height) and (0 <= c < self.width) and self[r, c] != None


class Piece(Field):
    def __init__(self, ch = None, descr = None):
        Field.__init__(self, 4, 4)
        if descr:
            for i in range(4):
                for j in range(4):
                    if descr[i][j] == '#':
                        self[i, j] = ch

    def rotated(self):
        res = Piece()
        for i in range(4):
            for j in range(4):
                res[3 - j, i] = self[i, j]
        return res


piece_data = {}
piece_data['I'] = Piece('I', [" #  ",
                              " #  ",
                              " #  ",
                              " #  "])
piece_data['J'] = Piece('J', ["  # ",
                              "  # ",
                              " ## ",
                              "    "])
piece_data['L'] = Piece('L', [" #  ",
                              " #  ",
                              " ## ",
                              "    "])
piece_data['T'] = Piece('T', ["    ",
                              " #  ",
                              "### ",
                              "    "])
piece_data['O'] = Piece('O', ["    ",
                              " ## ",
                              " ## ",
                              "    "])
piece_data['S'] = Piece('S', ["    ",
                              " ## ",
                              "##  ",
                              "    "])
piece_data['Z'] = Piece('Z', ["    ",
                              "##  ",
                              " ## ",
                              "    "])

class GameField(Field):
    def __init__(self):
        Field.__init__(self, 20, 10)
        self.piece = None
        self.prow = 0
        self.pcol = 0
        self.animation_end = -1

    def __getitem__(self, rc):
        r, c = rc
        if self.piece:
            i, j = r - self.prow, c - self.pcol
            if self.piece.contains(i, j):
                return self.piece[i, j]
        return Field.__getitem__(self, rc)

    def fixate(self):
        assert self.piece != None
        for i, j in self.piece.cells():
            y, x = self.prow + i, self.pcol + j
            if y < 0 or x < 0 or y >= self.height or x >= self.width:
                return False
        for i, j in self.piece.cells():
            y, x = self.prow + i, self.pcol + j
            self[y, x] = self.piece[i, j]
        self.piece = None
        return True

    def introduce(self, new_piece = None):
        assert self.piece == None
        if new_piece == None:
            new_piece = random.choice(list(piece_data.values()))
        self.piece = new_piece
        self.pcol = (self.width - new_piece.width) // 2
        self.prow = -(max(j for i, j in new_piece.cells()) + 1)

    def _check_move(self, p, pr, pc):
        for i, j in p.cells(pr, pc):
            if j < 0 or i >= self.height or j >= self.width: return False
            if i >= 0 and Field.__getitem__(self, (i, j)) != None: return False
        return True

    def _move(self, dr, dc, rot = 0):
        if self.piece == None: return False
        piece = self.piece
        for each in range(rot):
            piece = piece.rotated()
        if not self._check_move(piece, self.prow + dr, self.pcol + dc):
            return False
        else:
            self.piece = piece
            self.prow += dr
            self.pcol += dc
            return True

    def move_down(self):
        return self._move(1, 0)

    def move_left(self):
        return self._move(0, -1)

    def move_right(self):
        return self._move(0, 1)

    def rotate(self):
        return self._move(0, 0, 1)

    def clear_row(self, r):
        for y in range(r-1, -1, -1):
            for x in range(self.width):
                self[y+1, x] = self[y, x]
        for x in range(self.width):
            self[0, x] = None


class Game:
    def __init__(self):
        self.reset_game()
        self.cell_size_pixels = 50

    def reset_game(self):
        self.field = GameField()
        self.field.introduce()
        self.over = False
        self.exit_flag = False
        self.score = 0
        self.paused = False

    def screen_size(self):
        width = self.field.width * self.cell_size_pixels + 11
        height = self.field.height * self.cell_size_pixels + 11
        return (width, height)

    def show_scores(self, final = False):
        print("%s: %s lines" % (("Final score" if final else "Score"), self.score))

    def draw_field(self, field, x0, y0):
        size = self.cell_size_pixels
        r = pygame.Rect(x0 - 4, y0 - 4, size * field.width + 7, size * field.height + 7)
        self.field_rect = r
        pygame.draw.rect(self.screen, (128, 128, 204), r, 4)
        for i in range(field.height):
            for j in range(field.width):
                light_color = piece_color[field[i, j]]
                dark_color = make_dark_color(light_color)
                r = pygame.Rect(x0 + j*size, y0 + i*size, size, size)
                pygame.draw.rect(self.screen, dark_color, r)
                r = pygame.Rect(x0 + j*size + 1, y0 + i*size + 1, size - 2, size - 2)
                pygame.draw.rect(self.screen, light_color, r)

    def draw_messages(self, messages):
        for i in range(5):
            col = (255, 255, 255) if (i == 4) else (0, 0, 0)
            ss = map(
                lambda x: pygame.font.SysFont('Sans', x[1]).render(x[0], True, col),
                messages)
            dx = (1, 1, -1, -1, 0)[i]
            dy = (1, -1, 1, -1, 0)[i]
            y = self.field_rect.centery - sum(s.get_height() for s in ss) // 2
            for s in ss:
                r = pygame.Rect(
                    dx + self.field_rect.centerx - s.get_width() // 2,
                    dy + y, s.get_width(), s.get_height())
                self.screen.blit(s, r)
                y += s.get_height()

    def redraw(self):
        self.screen.fill((0, 0, 0))
        self.draw_field(self.field, 4, 4)

        if self.over:
            self.draw_messages([
                ('GAME OVER', 40),  ('Score: %s lines' % self.score, 25),  (' ', 20),
                ('Press spacebar to start', 15)])
        elif self.paused:
            self.draw_messages([('[Paused]', 30)])

        pygame.display.flip()

    def clear_rows(self):
        cleared = [0] * self.field.height
        for y in range(self.field.height):
            if all(self.field[y, x] != None for x in range(self.field.width)):
                cleared[y] = 1
                for x in range(self.field.width):
                    self.field[y, x] = '@'

        if sum(cleared) == 0:
            return

        self.score += sum(cleared)
        self.show_scores()

        # animation
        n = 6
        for i in range(n):
            piece_color['@'] = ((255,220,220), (180,150,150))[i % 2]
            self.redraw()
            time.sleep(0.20)
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.exit_flag = True

        for y in range(self.field.height):
            if cleared[y]:
                self.field.clear_row(y)
        self.redraw()

    def tick(self):
        if self.over:
            pass
        elif self.field.move_down():
            pass
        elif self.field.fixate():
            self.clear_rows()
            self.field.introduce()
        else:
            self.over = True
            self.show_scores(final = True)
            print("***GAME OVER***")

    def key_press(self, key):
        if key == pygame.K_ESCAPE:
            if self.paused:
                self.paused = False
                self.redraw()
            else:
                self.exit_flag = True
        elif self.over:
            if key in (pygame.K_SPACE, pygame.K_RETURN):
                self.reset_game()
                print()
        elif self.paused:
            if key in (pygame.K_SPACE, pygame.K_RETURN, pygame.K_p):
                self.paused = False
        else:
            if key == pygame.K_LEFT:
                self.field.move_left()
            elif key == pygame.K_RIGHT:
                self.field.move_right()
            elif key == pygame.K_DOWN:
                self.tick()
            elif key in (pygame.K_SPACE, pygame.K_TAB, pygame.K_RETURN):
                self.field.rotate()
            elif key == pygame.K_p:
                self.paused = True
            else:
                return
            self.redraw()

    def run(self):
        self.screen = pygame.display.set_mode(self.screen_size(), pygame.DOUBLEBUF)
        pygame.display.set_caption('Tetris')
        pygame.key.set_repeat(250, 25)
        pygame.time.set_timer(pygame.USEREVENT, 550)
        self.redraw()

        while not self.exit_flag:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    return
                elif event.type == pygame.USEREVENT:
                    if not self.over and not self.paused:
                        self.tick()
                        self.redraw()
                elif event.type == pygame.KEYDOWN:
                    self.key_press(event.key)
            time.sleep(1.0 / 100)


if __name__ == '__main__':
    pygame.init()
    try:
        Game().run()
    finally:
        pygame.quit()
