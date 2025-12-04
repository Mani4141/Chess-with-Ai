CMPM 123 – Chess AI: Negamax + Magic Bitboards

This project extends my chess engine to include full move generation with magic bitboards and a computer opponent powered by Negamax with Alpha-Beta pruning. The AI is implemented generically so either color can be automated, but for this assignment Black plays as the AI.

Features Implemented (Meeting Assignment Requirements)

Move generation for king, knight, pawn (existing)	✔	Fully working
Magic bitboards for rook, bishop, queen	✔	Fast sliding move generation
Negamax search	✔	Single unified min/max logic
Alpha-beta pruning	✔	Major search performance boost
Depth ≥ 3	✔	Depth 5 in Release build
Board evaluation function	✔	Material + positional bonuses
AI plays better than random	✔	Can beat a 1300-rated human
AI supports either color	✔	Currently Black is AI

AI Logic
Negamax with Alpha-Beta Pruning

Negamax simplifies minimax by using one scoring function for both players:

Value is always returned from the POV of the player to move

Child nodes are negated (-negamax(...))

Opponent’s turn flips sign (WHITE = +1, BLACK = -1)

Alpha-beta prunes unnecessary branches

This allows search far deeper than brute-force.
Typical computation per turn:

~0.5M – 9M positions using alpha-beta pruning
(depth 5 in Release mode)

Board Evaluation

White advantage = positive score
Black advantage = negative score

Components:

Component	Weight
Pawn	100
Knight	200
Bishop	230
Rook	400
Queen	900
King	2000

Plus:
Centralization bonus for knights & bishops
Slight incentives toward active piece usage
Removes early-game rook shuffling

♟ Move Generation

Fast, branchless bitboard-based movement:

Piece	Generation Method
Pawn	Manual-coded logic
Knight	Precomputed attack masks
King	Precomputed attack masks
Bishop	Magic bitboards (diagonal sliding)
Rook	Magic bitboards (rank/file sliding)
Queen	Bishop + Rook magic

Magic bitboards substantially improve search performance over simple ray scanning.

AI Playing Strength

I tested the AI as White vs. the engine as Black:

Opens with knights and bishops actively toward center

Captures unprotected material consistently

Converts material leads into wins

Rarely falls into simple tactics

I am ~1300 ELO on Chess.com rapid, and the AI has beaten me.
It feels competitive and non-random — a real chess opponent (me).

🛠 Performance Summary
Build	Depth	Smooth?	Notes
Release	5	✔✔	Fully meets rubric and feels like a good player

Sample move counts:

Moves checked: 694281 ... 9312166
best score values displayed live in console

🧩 Development Notes & Challenges
Issue	Solution
Wrong move list reused inside search	Generate moves fresh each recursion
Scores originally favored Black positive	Reversed sign convention white = positive
Early rook dancing	Centralization heuristics improved
Search too slow at depth 5	Enabled correct alpha-beta pruning



Video of chess gameplay:
[Video](https://youtu.be/HjeFj1fmosc)



Conclusion

Through this project I was able to successfully implement:

Full legal piece movement using magic bitboards
A working Chess AI that uses Negamax with alpha-beta pruning
Search depth of 5 in Release mode for strong, real-time play
An evaluation function that combines material values with positional bonuses

Overall, the AI plays intelligently and has even beaten me (I’m around 1300 rapid on Chess.com). I’m very happy with the results and I believe I have fully met and in some areas exceeded the requirements of this assignment and in general found this a really cool project!