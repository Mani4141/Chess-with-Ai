#include "Chess.h"
#include <limits>
#include <cmath>
#include <cctype>

Chess::Chess()
{
    _grid = new Grid(8, 8);
}

Chess::~Chess()
{
    delete _grid;
}

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

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();

    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);

    // Ensure pieceNotation()/stateString() work by tagging the piece type + color.
    // Expected tags:
    //   white: 1..6  (P,N,B,R,Q,K)
    //   black: 129..134  (p,n,b,r,q,k)
    int tag = static_cast<int>(piece);          // Pawn=1 .. King=6
    if (playerNumber == 1) tag += 128;          // black offset
    bit->setGameTag(tag);

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");

    // Start game first (some engines clear/prepare holders here),
    // then lay down pieces from FEN so nothing wipes them afterward.
    startGame();

    // Starting position (board-only FEN also accepted)
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");
}

void Chess::FENtoBoard(const std::string& fen) {
    // 1) Extract the board portion (field 1 of FEN). If only one field is given, use it directly.
    std::string boardField;
    {
        size_t sp = fen.find(' ');
        boardField = (sp == std::string::npos) ? fen : fen.substr(0, sp);
    }

    // 2) Validate 8 ranks (7 slashes)
    int slashCount = 0;
    for (char c : boardField) if (c == '/') ++slashCount;
    if (slashCount != 7) return; // malformed; ignore

    // 3) Clear existing pieces
    _grid->forEachSquare([](ChessSquare* sq, int /*x*/, int /*y*/) {
        sq->destroyBit();
    });

    // 4) Parse ranks: FEN rank 8 -> y=0 (top), ... rank 1 -> y=7 (bottom)
    int y = 0; // current rank row
    int x = 0; // file 0..7 == a..h

    auto placePiece = [&](char pch, int file, int rank) {
    int player = std::isupper(static_cast<unsigned char>(pch)) ? 0 : 1;
    char l = static_cast<char>(std::tolower(static_cast<unsigned char>(pch)));

    ChessPiece pieceEnum;
    switch (l) {
        case 'p': pieceEnum = Pawn;   break;
        case 'n': pieceEnum = Knight; break;
        case 'b': pieceEnum = Bishop; break;
        case 'r': pieceEnum = Rook;   break;
        case 'q': pieceEnum = Queen;  break;
        case 'k': pieceEnum = King;   break;
        default:  return;
    }

    ChessSquare* sq = _grid->getSquare(file, rank);
    if (!sq) return;

    Bit* bit = PieceForPlayer(player, pieceEnum);
    sq->setBit(bit);

    // NEW: center the sprite on its square
    // (Grid uses top-left origin; squares are pieceSize x pieceSize)
    const float cx = pieceSize * file + pieceSize * 0.5f;
    const float cy = pieceSize * rank + pieceSize * 0.5f;

    // Use whichever API your Bit supports. One of these will exist:
    // bit->setPosition(ImVec2(cx, cy));
    // bit->setPosition(cx, cy);
    // bit->setPos(cx, cy);
    // If your ChessSquare exposes its holder position, you can also do:
    // bit->setPosition(sq->holderPosition());
    bit->setPosition(ImVec2(cx, cy)); // <-- try this first
};


    for (size_t i = 0; i < boardField.size(); ++i) {
        char c = boardField[i];
        if (c == '/') {
            if (x != 8) return; // malformed rank width
            ++y; x = 0;
            if (y > 7) return;  // too many ranks
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            int run = c - '0';
            if (run < 1 || run > 8 || x + run > 8) return;
            x += run; // skip empty squares
            continue;
        }

        switch (std::tolower(static_cast<unsigned char>(c))) {
            case 'p': case 'n': case 'b': case 'r': case 'q': case 'k':
                if (x > 7) return;
                placePiece(c, x, y);
                ++x;
                break;
            default:
                return; // invalid character
        }
    }

    // 5) Final rank width check
    if (y != 7 || x != 8) return;

    // Fields 2..6 (active color, castling, ep, clocks) are accepted but ignored here.
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    // need to implement friendly/unfriendly in bit so for now this hack
    int currentPlayer = getCurrentPlayer()->playerNumber() * 128;
    int pieceColor = bit.gameTag() & 128;
    if (pieceColor == currentPlayer) return true;
    return false;
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    return true;
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        s += pieceNotation(x, y);
    });
    return s;
}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char playerNumber = s[index] - '0';
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1, Pawn));
        } else {
            square->setBit(nullptr);
        }
    });
}
