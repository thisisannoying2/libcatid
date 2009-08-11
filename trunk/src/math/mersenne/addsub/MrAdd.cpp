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

#include <cat/math/BigPseudoMersenne.hpp>
#include <cat/asm/big_x64_asm.hpp>
using namespace cat;

void BigPseudoMersenne::MrAddX(Leg *inout, Leg x)
{
    // If the addition overflowed, add C
    if (AddX(inout, x))
        AddX(inout, modulus_c);
}

void BigPseudoMersenne::MrAdd(const Leg *in_a, const Leg *in_b, Leg *out)
{
#if defined(CAT_USE_LEGS_ASM64)
    if (library_legs == 4)
    {
        bpm_add_4(modulus_c, in_a, in_b, out);
        return;
    }
#endif

    // If the addition overflowed, add C
    if (Add(in_a, in_b, out))
        AddX(out, modulus_c);
}

void BigPseudoMersenne::MrDouble(const Leg *in, Leg *out)
{
    // If the doubling overflowed, add C
    if (Double(in, out))
        AddX(out, modulus_c);
}