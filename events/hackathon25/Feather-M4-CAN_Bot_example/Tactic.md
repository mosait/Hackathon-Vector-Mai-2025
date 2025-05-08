# Game Tactic: Advanced Territorial Heuristic Algorithm

## Overview

Our strategy implements a heuristic-based greedy algorithm optimized for the Tron-style game where players leave trails behind them, wrap around walls, and die upon 180° turns or collisions with trails.

## Core Strategic Elements

### 1. Space Control Analysis

We use flood-fill calculations to determine the amount of accessible space from each potential move position. The algorithm strongly favors moves that maximize our accessible territory while avoiding situations that could trap us in smaller regions.

### 2. Opponent Prediction and Avoidance

Our algorithm:

- Tracks all opponent positions and movement patterns
- Predicts opponent trajectories based on their historical moves
- Assigns high penalties to moves that could result in collisions with predicted opponent paths
- Maintains strategic distance from opponents to reduce collision risk

### 3. Wall-Following Optimization

Taking advantage of the grid wrap-around mechanics, our algorithm implements a selective wall-following approach to:

- Maximize efficiency of path creation
- Create natural barriers that opponents must avoid
- Establish territory control along grid edges

### 4. Move Scoring System

Each potential move is evaluated with a sophisticated scoring function that balances:

- Available open space (10× weight)
- Distance to opponents (0.5× weight)
- Wall adjacency (5× bonus for having at least one adjacent wall)
- Direction consistency (5× bonus for continuing in same direction)
- Heavy penalties for illegal 180° turns (-2000 points)
- Penalties for potential collision with opponents' predicted moves

### 5. Adaptive Emergency Behavior

When all evaluated moves score poorly (below -500), the algorithm switches to a simple survival mode that seeks any valid move, prioritizing those that don't result in immediate death.

## Implementation Details

The algorithm is implemented with careful consideration of the Feather M4 CAN's resource constraints:

1. **Efficient Data Structures**

   - Boolean grid representation of the game state
   - Compact player trace vectors
   - Minimal memory allocation during computation

2. **Optimized Computation**

   - Limited-depth evaluation
   - Early termination of dead-end calculations
   - Reuse of previously calculated values where possible

3. **Balanced Performance**
   - Each game state update triggers a full evaluation of all possible moves
   - The scoring system adapts to changing game conditions
   - Computations complete within the 100ms game state update window

## Algorithmic Edge

Our implementation outperforms simpler strategies by:

- Thinking multiple moves ahead through space evaluation
- Adapting to opponent behaviors through prediction
- Balancing both offensive (territory control) and defensive (survival) play
- Optimizing movement patterns for the wrap-around grid mechanics

This blend of territorial control, opponent prediction, and adaptive decision-making creates a highly competitive player in the Vector Hackathon Tron-style game.
