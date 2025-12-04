#pragma once
#include "Bitboard.h"
#include "Game.h"
#include "Grid.h"
#include <vector>
#include <string>

constexpr int pieceSize = 80;

enum ChessPiece
{
    NoPiece,
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King
};

class Chess : public Game
{
public:
    Chess();
    ~Chess();

    void setUpBoard() override;

    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;

    bool gameHasAI() override { return true; }
    void updateAI() override;

    void stopGame() override;

    Player *checkForWinner() override;
    bool checkForDraw() override;

    void generateMovesForCurrentPlayer(BitMove* moves, int& count);

    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    Grid* getGrid() override { return _grid; }

private:
    // Internal piece-type enum just for movement logic
    enum PieceType {
        PT_None,
        PT_Pawn,
        PT_Knight,
        PT_Bishop,
        PT_Rook,
        PT_Queen,
        PT_King
    };

    struct PieceInfo {
        bool isWhite;
        PieceType type;
    };

    uint64_t getOccupancy() const;
    uint64_t getColorOccupancy(int player) const;
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;

    // Helpers for movement logic
    PieceInfo getPieceInfo(const Bit& bit) const;
    bool getCoordsForHolder(BitHolder& holder, int& x, int& y);
    int boardIndex(int x, int y) const;

    Grid* _grid;

    // === AI helpers ===
    std::vector<BitMove> generateAllMoves(const std::string& state, int playerColor);
    int evaluateBoard(const std::string& state);
    int negamax(std::string& state, int depth, int alpha, int beta, int playerColor);

    // For debugging: node count during a search
    int _countMoves = 0;

    // Root move list (current board)
    std::vector<BitMove> _moves;
};
