#include "Chess.h"
#include <limits>
#include <cmath>
#include <cctype>
#include "Bitboard.h"   // for BitMove + BitboardElement
#include "../imgui/imgui.h"
#include <iostream>
#include "MagicBitBoard.h"
#include <map>
#include <algorithm> // for std::max


// ===========================================================
// Constructor / Destructor
// ===========================================================

Chess::Chess()
{
    _grid = new Grid(8, 8);
    initMagicBitboards();
}

Chess::~Chess()
{
    cleanupMagicBitboards();
    delete _grid;
}

// ===========================================================
// Helpers
// ===========================================================

char Chess::pieceNotation(int x, int y) const
{
    const char *wpieces = { "0PNBRQK" };
    const char *bpieces = { "0pnbrqk" };
    Bit *bit = _grid->getSquare(x, y)->bit();
    char notation = '0';
    if (bit) {
        notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag() - 128];
    }
    return notation;
}

// Convert Bit.gameTag → PieceInfo
Chess::PieceInfo Chess::getPieceInfo(const Bit& bit) const
{
    PieceInfo info{};
    int tag = bit.gameTag();

    if (tag >= 128) {
        info.isWhite = false;
        tag -= 128;
    } else {
        info.isWhite = true;
    }

    switch (tag) {
        case 1: info.type = PT_Pawn;   break;
        case 2: info.type = PT_Knight; break;
        case 3: info.type = PT_Bishop; break;
        case 4: info.type = PT_Rook;   break;
        case 5: info.type = PT_Queen;  break;
        case 6: info.type = PT_King;   break;
        default: info.type = PT_None;  break;
    }

    return info;
}

// Scan grid to find coordinates of a BitHolder
bool Chess::getCoordsForHolder(BitHolder& holder, int& xOut, int& yOut)
{
    bool found = false;
    BitHolder* target = &holder;

    _grid->forEachSquare([&](ChessSquare* sq, int x, int y) {
        if ((BitHolder*)sq == target) {
            xOut = x;
            yOut = y;
            found = true;
        }
    });

    return found;
}

// Convert x,y to 0..63 (a1=0, h8=63)
int Chess::boardIndex(int x, int y) const
{
    int rank = 7 - y;
    int file = x;
    return rank * 8 + file;
}

namespace {
    inline bool isWhitePieceChar(char c) {
        return (c == 'P' || c == 'N' || c == 'B' ||
                c == 'R' || c == 'Q' || c == 'K');
    }

    inline bool isBlackPieceChar(char c) {
        return (c == 'p' || c == 'n' || c == 'b' ||
                c == 'r' || c == 'q' || c == 'k');
    }

    const int WHITE       =  1;
    const int BLACK       = -1;
    const int negInfinite = -10000000;
    const int posInfinite =  10000000;
}


// ===========================================================
// FEN Loader
// ===========================================================

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = {
        "pawn.png", "knight.png", "bishop.png",
        "rook.png", "queen.png", "king.png"
    };

    Bit* bit = new Bit();
    std::string spritePath = std::string("") +
        (playerNumber == 0 ? "w_" : "b_") +
        pieces[piece - 1];

    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);

    // Set gameTag so pieceNotation works
    int tag = static_cast<int>(piece);
    if (playerNumber == 1) tag += 128;
    bit->setGameTag(tag);

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    // Human = White (0), AI = Black (1)
    getPlayerAt(0)->setAIPlayer(false);
    getPlayerAt(1)->setAIPlayer(true);

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    startGame();

    // Standard starting position
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");
}

void Chess::FENtoBoard(const std::string& fen)
{
    std::string boardField;
    size_t sp = fen.find(' ');
    boardField = (sp == std::string::npos) ? fen : fen.substr(0, sp);

    int slashCount = 0;
    for (char c : boardField) if (c == '/') ++slashCount;
    if (slashCount != 7) return;

    _grid->forEachSquare([](ChessSquare* sq, int, int) { sq->destroyBit(); });

    int x = 0, y = 0;

    auto placePiece = [&](char pch, int file, int rank) {
        int player = std::isupper((unsigned char) pch) ? 0 : 1;
        char l = (char) std::tolower((unsigned char)pch);

        ChessPiece piece = NoPiece;
        switch (l) {
            case 'p': piece = Pawn;   break;
            case 'n': piece = Knight; break;
            case 'b': piece = Bishop; break;
            case 'r': piece = Rook;   break;
            case 'q': piece = Queen;  break;
            case 'k': piece = King;   break;
            default:  return;
        }

        ChessSquare* sq = _grid->getSquare(file, rank);
        if (!sq) return;

        Bit* bit = PieceForPlayer(player, piece);
        sq->setBit(bit);

        // IMPORTANT: tie the bit to this square like dropBitAtPoint does
        bit->setParent(sq);
        bit->moveTo(sq->getPosition());
    };

    for (char c : boardField)
    {
        if (c == '/') {
            if (x != 8) return;
            x = 0;
            y++;
            continue;
        }

        if (std::isdigit((unsigned char)c)) {
            x += (c - '0');
            continue;
        }

        placePiece(c, x, y);
        x++;
    }
}


// ===========================================================
// Movement Rules (Selection + Trying Moves)
// ===========================================================

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    int currentPlayer = getCurrentPlayer()->playerNumber() * 128;
    return (bit.gameTag() & 128) == currentPlayer;
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    if (!canBitMoveFrom(bit, src))
        return false;

    int sx, sy, dx, dy;
    if (!getCoordsForHolder(src, sx, sy)) return false;
    if (!getCoordsForHolder(dst, dx, dy)) return false;

    int dxSigned = dx - sx;
    int dySigned = dy - sy;
    int ax = std::abs(dxSigned);
    int ay = std::abs(dySigned);

    ChessSquare* dstSq = _grid->getSquare(dx, dy);
    Bit* dstBit = dstSq ? dstSq->bit() : nullptr;

    PieceInfo info = getPieceInfo(bit);
    bool isWhite = info.isWhite;

    // Cannot capture own color
    if (dstBit) {
        PieceInfo d = getPieceInfo(*dstBit);
        if (d.isWhite == isWhite)
            return false;
    }

    switch (info.type)
    {
        case PT_Pawn:
        {
            int dir = isWhite ? -1 : 1;
            int startY = isWhite ? 6 : 1;

            // forward move (no capture)
            if (dxSigned == 0)
            {
                if (dySigned == dir && !dstBit)
                    return true;

                if (sy == startY && dySigned == 2 * dir) {
                    ChessSquare* mid = _grid->getSquare(sx, sy + dir);
                    if (mid && !mid->bit() && !dstBit)
                        return true;
                }

                return false;
            }

            // diagonal capture
            if (ax == 1 && dySigned == dir && dstBit)
                return true;

            return false;
        }

        case PT_Knight:
            return (ax == 1 && ay == 2) || (ax == 2 && ay == 1);

        case PT_King:
            return (ax <= 1 && ay <= 1 && (ax + ay) != 0);

        // --------------------------
        // BISHOP: diagonals
        // --------------------------
        case PT_Bishop:
        {
            if (ax == ay && ax != 0) {
                int stepX = (dxSigned > 0) ? 1 : -1;
                int stepY = (dySigned > 0) ? 1 : -1;

                int cx = sx + stepX;
                int cy = sy + stepY;

                // Make sure path is clear
                while (cx != dx || cy != dy) {
                    ChessSquare* mid = _grid->getSquare(cx, cy);
                    if (mid && mid->bit())
                        return false; // blocked

                    cx += stepX;
                    cy += stepY;
                }

                // If we reach here, either dst is empty or enemy.
                return true;
            }
            return false;
        }

        // --------------------------
        // ROOK: ranks/files
        // --------------------------
        case PT_Rook:
        {
            if ((ax == 0 && ay != 0) || (ay == 0 && ax != 0)) {
                int stepX = (dxSigned == 0) ? 0 : (dxSigned > 0 ? 1 : -1);
                int stepY = (dySigned == 0) ? 0 : (dySigned > 0 ? 1 : -1);

                int cx = sx + stepX;
                int cy = sy + stepY;

                // Check path is clear
                while (cx != dx || cy != dy) {
                    ChessSquare* mid = _grid->getSquare(cx, cy);
                    if (mid && mid->bit())
                        return false; // blocked

                    cx += stepX;
                    cy += stepY;
                }

                return true;
            }
            return false;
        }

        // --------------------------
        // QUEEN: bishop or rook move
        // --------------------------
        case PT_Queen:
        {
            // Diagonal like bishop
            if (ax == ay && ax != 0) {
                int stepX = (dxSigned > 0) ? 1 : -1;
                int stepY = (dySigned > 0) ? 1 : -1;

                int cx = sx + stepX;
                int cy = sy + stepY;

                while (cx != dx || cy != dy) {
                    ChessSquare* mid = _grid->getSquare(cx, cy);
                    if (mid && mid->bit())
                        return false;

                    cx += stepX;
                    cy += stepY;
                }

                return true;
            }

            // Straight like rook
            if ((ax == 0 && ay != 0) || (ay == 0 && ax != 0)) {
                int stepX = (dxSigned == 0) ? 0 : (dxSigned > 0 ? 1 : -1);
                int stepY = (dySigned == 0) ? 0 : (dySigned > 0 ? 1 : -1);

                int cx = sx + stepX;
                int cy = sy + stepY;

                while (cx != dx || cy != dy) {
                    ChessSquare* mid = _grid->getSquare(cx, cy);
                    if (mid && mid->bit())
                        return false;

                    cx += stepX;
                    cy += stepY;
                }

                return true;
            }

            return false;
        }

        default:
            return false;
    }
}


// ===========================================================
// Bitboard Occupancy Helpers
// ===========================================================

uint64_t Chess::getOccupancy() const
{
    uint64_t occ = 0ULL;
    _grid->forEachSquare([&](ChessSquare* sq, int x, int y) {
        if (sq->bit()) occ |= (1ULL << boardIndex(x, y));
    });
    return occ;
}

uint64_t Chess::getColorOccupancy(int player) const
{
    uint64_t occ = 0ULL;
    _grid->forEachSquare([&](ChessSquare* sq, int x, int y) {
        Bit* b = sq->bit();
        if (b && b->getOwner()->playerNumber() == player)
            occ |= (1ULL << boardIndex(x, y));
    });
    return occ;
}


// ===========================================================
// Move Generator (Pawn, Knight, Bishop, Rook, Queen, King)
// using Magic Bitboards
// ===========================================================

void Chess::generateMovesForCurrentPlayer(BitMove* moves, int& count)
{
    count = 0;
    int player = getCurrentPlayer()->playerNumber();

    // Bitboard occupancies
    uint64_t occ = getOccupancy();
    uint64_t own = getColorOccupancy(player);

    _grid->forEachSquare([&](ChessSquare* sq, int x, int y)
    {
        Bit* bit = sq->bit();
        if (!bit) return;
        if (bit->getOwner()->playerNumber() != player) return;

        PieceInfo info = getPieceInfo(*bit);
        int from = boardIndex(x, y);

        // -------------------------------
        // PAWN MOVES
        // -------------------------------
        if (info.type == PT_Pawn)
        {
            int dir       = info.isWhite ? -1 : 1;   // white goes "up", black "down"
            int startRank = info.isWhite ? 6  : 1;   // rank index in your grid
            int ny        = y + dir;

            // ----- forward 1 -----
            if (ny >= 0 && ny < 8)
            {
                ChessSquare* one = _grid->getSquare(x, ny);
                if (one && !one->bit())
                {
                    moves[count++] = BitMove(from, boardIndex(x, ny), Pawn);

                    // ----- forward 2 from starting rank -----
                    if (y == startRank) {
                        int ny2 = y + 2 * dir;
                        if (ny2 >= 0 && ny2 < 8) {
                            ChessSquare* mid = _grid->getSquare(x, y + dir);
                            ChessSquare* two = _grid->getSquare(x, ny2);
                            if (mid && two && !mid->bit() && !two->bit()) {
                                moves[count++] = BitMove(from, boardIndex(x, ny2), Pawn);
                            }
                        }
                    }
                }
            }

            // ----- capture diagonals -----
            int left  = x - 1;
            int right = x + 1;

            if (left >= 0 && ny >= 0 && ny < 8)
            {
                ChessSquare* c = _grid->getSquare(left, ny);
                if (c && c->bit() && c->bit()->getOwner()->playerNumber() != player)
                    moves[count++] = BitMove(from, boardIndex(left, ny), Pawn);
            }

            if (right < 8 && ny >= 0 && ny < 8)
            {
                ChessSquare* c = _grid->getSquare(right, ny);
                if (c && c->bit() && c->bit()->getOwner()->playerNumber() != player)
                    moves[count++] = BitMove(from, boardIndex(right, ny), Pawn);
            }
        }

        // -------------------------------
        // KNIGHT MOVES (magic table)
        // -------------------------------
        if (info.type == PT_Knight)
        {
            uint64_t attacks = KnightAttacks[from] & ~own;

            BitboardElement bb(attacks);
            bb.forEachBit([&](int to) {
                moves[count++] = BitMove(from, to, Knight);
            });
        }

        // -------------------------------
        // KING MOVES (magic table)
        // -------------------------------
        if (info.type == PT_King)
        {
            uint64_t attacks = KingAttacks[from] & ~own;

            BitboardElement bb(attacks);
            bb.forEachBit([&](int to) {
                moves[count++] = BitMove(from, to, King);
            });
        }

        // -------------------------------
        // SLIDING PIECES: BISHOP / ROOK / QUEEN
        // -------------------------------
        if (info.type == PT_Bishop)
        {
            uint64_t attacks = getBishopAttacks(from, occ) & ~own;

            BitboardElement bb(attacks);
            bb.forEachBit([&](int to) {
                moves[count++] = BitMove(from, to, Bishop);
            });
        }

        if (info.type == PT_Rook)
        {
            uint64_t attacks = getRookAttacks(from, occ) & ~own;

            BitboardElement bb(attacks);
            bb.forEachBit([&](int to) {
                moves[count++] = BitMove(from, to, Rook);
            });
        }

        if (info.type == PT_Queen)
        {
            uint64_t attacks = getQueenAttacks(from, occ) & ~own;

            BitboardElement bb(attacks);
            bb.forEachBit([&](int to) {
                moves[count++] = BitMove(from, to, Queen);
            });
        }

    });
}


// ===========================================================
// Move generation from state string (for negamax)
// ===========================================================

std::vector<BitMove> Chess::generateAllMoves(const std::string& state, int playerColor)
{
    std::vector<BitMove> moves;
    moves.reserve(32);

    // Build occupancy and "own" bitboard from the state string
    uint64_t occ = 0ULL;
    uint64_t own = 0ULL;

    for (int sq = 0; sq < 64; ++sq) {
        char c = state[sq];
        if (c == '0') continue;

        uint64_t mask = 1ULL << sq;
        occ |= mask;

        bool isWhite = isWhitePieceChar(c);
        if ((playerColor == WHITE && isWhite) ||
            (playerColor == BLACK && !isWhite)) {
            own |= mask;
        }
    }

    // Now generate moves by scanning the board and using magic bitboards
    for (int sq = 0; sq < 64; ++sq) {
        char c = state[sq];
        if (c == '0') continue;

        bool isWhite = isWhitePieceChar(c);
        // Skip enemy pieces
        if ((playerColor == WHITE && !isWhite) ||
            (playerColor == BLACK && isWhite)) {
            continue;
        }

        char piece = (char)std::toupper((unsigned char)c);
        int file = sq % 8;
        int rank = sq / 8;

        // -------------------------------------------
        // PAWNS
        // -------------------------------------------
        if (piece == 'P') {
            if (playerColor == WHITE) {
                // One step forward
                int one = sq + 8;
                if (one < 64 && state[one] == '0') {
                    moves.emplace_back(sq, one, Pawn);

                    // Two steps from rank 2 (rank == 1)
                    if (rank == 1) {
                        int two = sq + 16;
                        if (two < 64 && state[two] == '0') {
                            moves.emplace_back(sq, two, Pawn);
                        }
                    }
                }

                // Captures
                if (file > 0) {
                    int cap = sq + 7;
                    if (cap < 64 && isBlackPieceChar(state[cap])) {
                        moves.emplace_back(sq, cap, Pawn);
                    }
                }
                if (file < 7) {
                    int cap = sq + 9;
                    if (cap < 64 && isBlackPieceChar(state[cap])) {
                        moves.emplace_back(sq, cap, Pawn);
                    }
                }
            }
            else { // BLACK
                // One step forward
                int one = sq - 8;
                if (one >= 0 && state[one] == '0') {
                    moves.emplace_back(sq, one, Pawn);

                    // Two steps from rank 7 (rank == 6)
                    if (rank == 6) {
                        int two = sq - 16;
                        if (two >= 0 && state[two] == '0') {
                            moves.emplace_back(sq, two, Pawn);
                        }
                    }
                }

                // Captures
                if (file > 0) {
                    int cap = sq - 9;
                    if (cap >= 0 && isWhitePieceChar(state[cap])) {
                        moves.emplace_back(sq, cap, Pawn);
                    }
                }
                if (file < 7) {
                    int cap = sq - 7;
                    if (cap >= 0 && isWhitePieceChar(state[cap])) {
                        moves.emplace_back(sq, cap, Pawn);
                    }
                }
            }

            continue; // done with pawn
        }

        // -------------------------------------------
        // KNIGHTS
        // -------------------------------------------
        if (piece == 'N') {
            uint64_t attacks = KnightAttacks[sq] & ~own;

            BitboardElement bb(attacks);
            bb.forEachBit([&](int to) {
                moves.emplace_back(sq, to, Knight);
            });
            continue;
        }

        // -------------------------------------------
        // KING
        // -------------------------------------------
        if (piece == 'K') {
            uint64_t attacks = KingAttacks[sq] & ~own;

            BitboardElement bb(attacks);
            bb.forEachBit([&](int to) {
                moves.emplace_back(sq, to, King);
            });
            continue;
        }

        // -------------------------------------------
        // BISHOP
        // -------------------------------------------
        if (piece == 'B') {
            uint64_t attacks = getBishopAttacks(sq, occ) & ~own;

            BitboardElement bb(attacks);
            bb.forEachBit([&](int to) {
                moves.emplace_back(sq, to, Bishop);
            });
            continue;
        }

        // -------------------------------------------
        // ROOK
        // -------------------------------------------
        if (piece == 'R') {
            uint64_t attacks = getRookAttacks(sq, occ) & ~own;

            BitboardElement bb(attacks);
            bb.forEachBit([&](int to) {
                moves.emplace_back(sq, to, Rook);
            });
            continue;
        }

        // -------------------------------------------
        // QUEEN
        // -------------------------------------------
        if (piece == 'Q') {
            uint64_t attacks = getQueenAttacks(sq, occ) & ~own;

            BitboardElement bb(attacks);
            bb.forEachBit([&](int to) {
                moves.emplace_back(sq, to, Queen);
            });
            continue;
        }
    }

    return moves;
}


// ===========================================================
// Negamax with alpha-beta
// ===========================================================

int Chess::negamax(std::string& state, int depth, int alpha, int beta, int playerColor)
{
    _countMoves++;

    // Base case: evaluate from side-to-move POV
    if (depth == 0) {
        // evaluateBoard is White-positive; multiply by playerColor
        return playerColor * evaluateBoard(state);
    }

    auto moves = generateAllMoves(state, playerColor);

    // Simple terminal handling: no legal moves -> treat as 0 (drawish)
    if (moves.empty()) {
        return 0;
    }

    int bestVal = negInfinite;

    for (auto move : moves) {
        // Save board
        char boardSave   = state[move.to];
        char pieceMoving = state[move.from];

        // Make move
        state[move.to]   = pieceMoving;
        state[move.from] = '0';

        // Recurse: other side to move, flip color, window, and sign
        int val = -negamax(state, depth - 1, -beta, -alpha, -playerColor);

        // Undo move
        state[move.from] = pieceMoving;
        state[move.to]   = boardSave;

        if (val > bestVal) {
            bestVal = val;
        }

        if (val > alpha) {
            alpha = val;
        }

        if (alpha >= beta) {
            break; // beta cutoff
        }
    }

    return bestVal;
}



// ===========================================================
// updateAI – root search + execute best move
// ===========================================================

void Chess::updateAI()
{
    int bestVal = negInfinite;
    BitMove bestMove;
    std::string state = stateString();
    _countMoves = 0;

    // Determine which side is to move at the root
    int currentPlayerIndex = getCurrentPlayer()->playerNumber(); // 0 = White, 1 = Black
    int rootColor = (currentPlayerIndex == 0) ? WHITE : BLACK;

    // Build root move list from the actual board position
    _moves.clear();
    _moves.reserve(32);

    BitMove temp[256];
    int count = 0;
    generateMovesForCurrentPlayer(temp, count);
    for (int i = 0; i < count; ++i) {
        _moves.push_back(temp[i]);
    }

    // Depth: 3 for debug, 5 for Release is good
    const int searchDepth = 5;

    for (auto move : _moves) {
        char boardSave   = state[move.to];
        char pieceMoving = state[move.from];

        // Make the move on our state copy (other side to move next)
        state[move.to]   = pieceMoving;
        state[move.from] = '0';

        // Negamax: after our move, call with -rootColor and negated window
        int moveVal = -negamax(
            state,
            searchDepth - 1,
            -posInfinite,
            -negInfinite,
            -rootColor
        );

        // Undo move
        state[move.from] = pieceMoving;
        state[move.to]   = boardSave;

        if (moveVal > bestVal) {
            bestVal = moveVal;
            bestMove = move;
        }
    }

    if (bestVal == negInfinite) {
        // No legal moves – probably mate or stalemate
        return;
    }

    std::cout << "Moves checked: " << _countMoves
              << "  eval: " << evaluateBoard(stateString())
              << std::endl;

    // Helper to convert 0..63 ↔ (x, y)
    auto squareToXY = [](int sq, int& x, int& y) {
        int file = sq % 8;
        int rank = sq / 8;
        x = file;
        y = 7 - rank;   // inverse of boardIndex(x, y)
    };

    int srcX, srcY, dstX, dstY;
    squareToXY(bestMove.from, srcX, srcY);
    squareToXY(bestMove.to,   dstX, dstY);

    BitHolder& src = getHolderAt(srcX, srcY);
    BitHolder& dst = getHolderAt(dstX, dstY);

    Bit* bit = src.bit();
    if (!bit) return;

    dst.dropBitAtPoint(bit, ImVec2(0, 0));
    src.setBit(nullptr);

    bitMovedFromTo(*bit, src, dst);
}




// ===========================================================
// Evaluation
// ===========================================================

int Chess::evaluateBoard(const std::string& state)
{
    // White pieces positive, black pieces negative (White POV)
    static const std::map<char, int> baseScores = {
        {'P', 100},  {'p', -100},
        {'N', 200},  {'n', -200},
        {'B', 230},  {'b', -230},
        {'R', 400},  {'r', -400},
        {'Q', 900},  {'q', -900},
        {'K', 2000}, {'k', -2000},
        {'0', 0}
    };

    auto pieceValue = [&](char ch) -> int {
        auto it = baseScores.find(ch);
        if (it != baseScores.end()) return it->second;
        return 0;
    };

    // Small centralization bonus for knights and bishops
    auto centralBonus = [](char ch, int sq) -> int {
        if (ch == '0') return 0;

        char u = (char)std::toupper((unsigned char)ch);
        if (u != 'N' && u != 'B') {
            return 0; // only knights and bishops get this
        }

        int file = sq % 8;    // 0..7
        int rank = sq / 8;    // 0..7

        // Distance from the "center files" (3 and 4) and "center ranks" (3 and 4)
        int df = std::min(std::abs(file - 3), std::abs(file - 4));
        int dr = std::min(std::abs(rank - 3), std::abs(rank - 4));

        int d = df + dr;          // 0 in very center, up to 6 in corners
        int base = std::max(0, 6 - d); // 0..6, never negative
        int bonus = base * 2;     // 0..12

        // White piece: positive bonus; Black piece: negative bonus (still White POV)
        if (std::isupper((unsigned char)ch)) {
            return bonus;
        } else {
            return -bonus;
        }
    };

    int value = 0;

    for (int sq = 0; sq < 64; ++sq) {
        char ch = state[sq];

        // Base material
        value += pieceValue(ch);

        // Positional tweak for knights/bishops
        value += centralBonus(ch, sq);
    }

    // >0 means White is better, <0 means Black is better
    return value;
}



// ===========================================================
// Game end check
// ===========================================================

Player* Chess::ownerAt(int x, int y) const
{
    if (!(x >= 0 && x < 8 && y >= 0 && y < 8)) return nullptr;
    ChessSquare* sq = _grid->getSquare(x, y);
    if (!sq || !sq->bit()) return nullptr;
    return sq->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
}


// ===========================================================
// State String Serialization
// ===========================================================

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    // 64-character string, initially empty board
    std::string s(64, '0');

    _grid->forEachSquare([&](ChessSquare* sq, int x, int y) {
        char c = pieceNotation(x, y);
        int idx = boardIndex(x, y);  // 0..63 index used by bitboards
        s[idx] = c;
    });

    return s;
}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* sq, int x, int y) {
        char c = s[y * 8 + x];
        if (c == '0') sq->setBit(nullptr);
    });
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int, int) {
        square->destroyBit();
    });
}
