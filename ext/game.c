/*
 * chess - a fast library to play chess in Ruby
 *
 * Copyright (c) 2011-2012, Enrico Pilotto <enrico@megiston.it>
 * This code is under LICENSE LGPLv3
 */

#include "game.h"

static Board STARTING_BOARD;

void
init_chess_library ()
{
  init_board (&STARTING_BOARD);
  precalculate_all_xray ();
}

Game*
init_game ()
{
  Game *g = NEW_GAME;
  g->current = 0;
  g->result = IN_PROGRESS;
  return g;
}

// Free memory of a game
void
free_game (Game *g)
{
  int i;
  for (i = 0; i < g->current; i++)
    {
      free (g->boards[i]);
      free (g->moves[i]);
      free (g->full_moves[i]);
    }
  free (g);
}

Board*
current_board (Game *g)
{
  if (g->current > 0)
    return g->boards[g->current-1];
  return &STARTING_BOARD;
}

Board*
get_board (Game *g, int index)
{
  if (index < 0)
    return &STARTING_BOARD;
  if (index < g->current)
    return g->boards[index];
  return 0;
}

char*
current_move (Game *g)
{
  if (g->current > 0)
    return g->moves[g->current-1];
  return 0;
}

char*
current_full_move (Game *g)
{
  if (g->current > 0)
    return g->full_moves[g->current-1];
  return 0;
}

bool
apply_move (Game *g, int from, int to, char promote_in)
{
  if (g->result != IN_PROGRESS) return FALSE;
  Board *board = current_board (g);
  Board *new_board = NEW_BOARD;
  char capture = 0;
  char *move_done = castling (board, castling_type (board, from, to), new_board);
  if (!move_done) // If move is not a castling
    {
      if (!try_move (board, from, to, promote_in, new_board, &move_done, &capture))
        {
          free (new_board);
          return FALSE;
        }
    }
  // Ok move is legal, update the game
  update_castling (new_board, from);
  update_en_passant (new_board, from, to);
  if (new_board->active_color)
    new_board->fullmove_number++;
  new_board->active_color = !new_board->active_color;
  if (capture || toupper (board->placement[from]) == 'P')
    new_board->halfmove_clock = 0;
  else
    new_board->halfmove_clock++;
  g->boards[g->current] = new_board;
  g->moves[g->current] = move_done;
  g->full_moves[g->current] = ft_to_full_move (from, to, promote_in);
  g->current++;
  // Test check or checkmate of opponent king
  if (king_in_check (new_board, new_board->active_color))
    {
      if (king_in_checkmate (new_board, new_board->active_color))
        {
          strcat (move_done, "#");
          g->result = !new_board->active_color;
        }
      else
        strcat (move_done, "+");
    }
  // Test insufficient material
  else if (insufficient_material (new_board))
    g->result = DRAW;
  // Test stalemate
  else if (stalemate (new_board, new_board->active_color))
    g->result = DRAW;
  return TRUE;
}

void
rollback (Game *g)
{
  if (g->current > 0)
    {
      g->current--;
      free (g->boards[g->current]);
      free (g->moves[g->current]);
      free (g->full_moves[g->current]);
    }
}

bool
threefold_repetition (Game *g)
{
  char placement[65];
  char *fen, *castling, *ep;
  char* s[g->current];
  int i, j;
  bool found = FALSE;
  for (i = 0; i < g->current; i++)
    {
      fen = to_fen (g->boards[i]);
      for (j = 0; fen[j] != ' '; j++)
        placement[j] = fen[j];
      placement[j] = '\0';
      s[i] = (char *) malloc (80);
      castling = castling_to_s (g->boards[i]->castling);
      ep = en_passant_to_s (g->boards[i]->en_passant);
      sprintf (s[i], "%s %s %s", placement, castling, ep);
      free (fen);
      free (castling);
      free (ep);
    }
  qsort (s, g->current, sizeof (char *), compare);
  for (i = 0; i < g->current-2; i++)
    {
      if (strcmp (s[i], s[i+1]) == 0 && strcmp (s[i], s[i+2]) == 0)
        {
          found = TRUE;
          break;
        }
      free (s[i]);
    }
  free (s[g->current-1]);
  free (s[g->current-2]);
  return found;
}

void
set_fen (Game *g, const char *fen)
{
  Board *board = NEW_BOARD;
  int i = 0, j, k, pos, square;
  char *pch;
  char *s = (char *) malloc (strlen (fen));
  strcpy (s, fen);
  // Init board
  memset (board->placement, '\0', 64);
  board->pawns[WHITE]   = 0x0;
  board->pawns[BLACK]   = 0x0;
  board->rooks[WHITE]   = 0x0;
  board->rooks[BLACK]   = 0x0;
  board->knights[WHITE] = 0x0;
  board->knights[BLACK] = 0x0;
  board->bishops[WHITE] = 0x0;
  board->bishops[BLACK] = 0x0;
  board->queens[WHITE]  = 0x0;
  board->queens[BLACK]  = 0x0;
  board->king[WHITE]    = 0x0;
  board->king[BLACK]    = 0x0;
  pch = strtok (s, " /");
  while (pch != NULL)
    {
      if (i < 8)
        {
          for (j = 0, k = 0; j < (int) strlen (pch); j++)
            {
              if (pch[j] - 48 > 8)
                {
                  square = (abs (i - 7) * 8) + k;
                  switch (pch[j])
                    {
                      case 'P':
                      board->placement[square] = pch[j];
                      board->pawns[WHITE] ^= 1ULL << square;
                      break;
                      case 'p':
                      board->placement[square] = pch[j];
                      board->pawns[BLACK] ^= 1ULL << square;
                      break;
                      case 'R':
                      board->placement[square] = pch[j];
                      board->rooks[WHITE] ^= 1ULL << square;
                      break;
                      case 'r':
                      board->placement[square] = pch[j];
                      board->rooks[BLACK] ^= 1ULL << square;
                      break;
                      case 'N':
                      board->placement[square] = pch[j];
                      board->knights[WHITE] ^= 1ULL << square;
                      break;
                      case 'n':
                      board->placement[square] = pch[j];
                      board->knights[BLACK] ^= 1ULL << square;
                      break;
                      case 'B':
                      board->placement[square] = pch[j];
                      board->bishops[WHITE] ^= 1ULL << square;
                      break;
                      case 'b':
                      board->placement[square] = pch[j];
                      board->bishops[BLACK] ^= 1ULL << square;
                      break;
                      case 'Q':
                      board->placement[square] = pch[j];
                      board->queens[WHITE] ^= 1ULL << square;
                      break;
                      case 'q':
                      board->placement[square] = pch[j];
                      board->queens[BLACK] ^= 1ULL << square;
                      break;
                      case 'K':
                      board->placement[square] = pch[j];
                      board->king[WHITE] ^= 1ULL << square;
                      break;
                      case 'k':
                      board->placement[square] = pch[j];
                      board->king[BLACK] ^= 1ULL << square;
                      break;
                    }
                  k++;
                }
              else
                k += pch[j] - 48;
            }
        }
      else if (i == 8)
        {
          board->active_color = pch[0] == 'b';
        }
      else if (i == 9)
        {
          board->castling = 0x0000;
          for (j = 0; j < (int) strlen (pch); j++)
            {
              switch (pch[j])
                {
                  case 'K':
                  board->castling |= 0x1000;
                  break;
                  case 'Q':
                  board->castling |= 0x0100;
                  break;
                  case 'k':
                  board->castling |= 0x0010;
                  break;
                  case 'q':
                  board->castling |= 0x0001;
                  break;
                }
            }
        }
      else if (i == 10)
        {
          board->en_passant = coord_to_square (pch);
        }
      else if (i == 11)
        {
          board->halfmove_clock = atoi (pch);
        }
      else if (i == 12)
        {
          board->fullmove_number = atoi (pch);
        }
      i++;
      pch = strtok (NULL, " /");
    }
  set_occupied (board);
  g->boards[g->current] = board;
  g->moves[g->current] = (char *) malloc (11);
  g->full_moves[g->current] = (char *) malloc (11);
  strcpy (g->moves[g->current], "SET BY FEN");
  strcpy (g->full_moves[g->current], "SET BY FEN");
  g->current++;
  free (s);
  free (pch);
}

///////////////////////////////////
// MAIN (only for internal test) //
///////////////////////////////////
int
main ()
{
  // Valgrind run
  init_chess_library ();
  int i, from, to;

  for (i = 0; i < 1000; i++)
    {
      Game *g = init_game ();
      Board *board;
      char *fen;

      // 1. e4 a6 2. Bc4 a5 3. Qh5 a4 4. Qxf7#
      board = current_board (g);
      get_coord (board, 'P', "e", "e4", 0, &from, &to);
      pseudo_legal_move (board, from, to);
      apply_move (g, from, to, 0);
      fen = to_fen (board);
      free (fen);

      board = current_board (g);
      get_coord (board, 'P', "a", "a6", 0, &from, &to);
      pseudo_legal_move (board, from, to);
      apply_move (g, from, to, 0);
      fen = to_fen (current_board (g));
      free (fen);

      board = current_board (g);
      get_coord (board, 'B', "", "c4", 0, &from, &to);
      pseudo_legal_move (board, from, to);
      apply_move (g, from, to, 0);
      fen = to_fen (current_board (g));
      free (fen);

      board = current_board (g);
      get_coord (board, 'P', "a", "a5", 0, &from, &to);
      pseudo_legal_move (board, from, to);
      apply_move (g, from, to, 0);
      fen = to_fen (current_board (g));
      free (fen);

      board = current_board (g);
      get_coord (board, 'Q', "", "h5", 0, &from, &to);
      pseudo_legal_move (board, from, to);
      apply_move (g, from, to, 0);
      fen = to_fen (current_board (g));
      free (fen);

      board = current_board (g);
      get_coord (board, 'P', "a", "a4", 0, &from, &to);
      pseudo_legal_move (board, from, to);
      apply_move (g, from, to, 0);
      fen = to_fen (current_board (g));
      free (fen);

      board = current_board (g);
      get_coord (board, 'Q', "", "f7", 0, &from, &to);
      pseudo_legal_move (board, from, to);
      apply_move (g, from, to, 0);
      fen = to_fen (current_board (g));
      free (fen);

      free_game (g);
    }
  return 0;
}
