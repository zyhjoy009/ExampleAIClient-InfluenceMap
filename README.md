# ExampleAIClient

A starcraft AI project

In this project, I try to design a learnable starcraft AI with reinformcement learning, influence map, potential fields, etc

Version 1 Self units only move to the opposite side of map and attack enemies during the route.

Version 2 I use 6 influence maps to design individual unit's policy, including enemy attract/repel, own attract/repel, terrain repel, and goal attract maps. The maps are determined by relative distances. Mainly in linear relationship with the distances, and some of them use maximum operator to handle multiple attract/repel sources, while some use sum operator. Next I want to use reinforcement learning to learn these weights and parameters.
