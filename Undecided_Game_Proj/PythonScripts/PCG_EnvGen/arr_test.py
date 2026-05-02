import sys
import numpy as np
from itertools import product

GridSize = 256

def FlattenCoordinate(x, y, z):
    return x + (y * GridSize) + (z * GridSize * GridSize)

for i, j, k in product(range(4), repeat=3):

    x = i + 2
    y = j + 51
    z = k + 124

    print("xV0: ", FlattenCoordinate(x + 0, y + 0, z + 0))
    print("xV1: ", FlattenCoordinate(x + 0, y + 0, z + 1))
    print("xV2: ", FlattenCoordinate(x + 0, y + 1, z + 1))
    print("xV3: ", FlattenCoordinate(x + 0, y + 1, z + 0))

print("Max grid size: ", pow(GridSize + 1, 3), "\n"
      "Size in bytes: ", 4 * pow(GridSize + 1, 3))