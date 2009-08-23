/*
    Copyright 2009 Christopher A. Taylor

    This file is part of LibCat.

    LibCat is free software: you can redistribute it and/or modify
    it under the terms of the Lesser GNU General Public License as
    published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    LibCat is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    Lesser GNU General Public License for more details.

    You should have received a copy of the Lesser GNU General Public
    License along with LibCat.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cat/math/BigTwistedEdwards.hpp>
using namespace cat;

// out(x,y) = (X/Z,Y/Z)
void BigTwistedEdwards::SaveAffineXY(const Leg *in, void *out_x, void *out_y)
{
    // If the coordinates are already normalized to affine, then we can just save what is there
    if (EqualX(in+ZOFF, 1))
    {
        Save(in+XOFF, out_x, RegBytes());
        Save(in+YOFF, out_y, RegBytes());
    }
    else
    {
        // A = 1 / in.Z
        MrInvert(in+ZOFF, A);

        // B = A * in.X
        MrMultiply(in+XOFF, A, B);
        MrReduce(B);
        Save(B, out_x, RegBytes());

        // C = A * in.Y
        MrMultiply(in+YOFF, A, C);
        MrReduce(C);
        Save(C, out_y, RegBytes());
    }
}
