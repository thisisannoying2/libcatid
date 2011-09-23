/*
	Copyright (c) 2009-2010 Christopher A. Taylor.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of LibCat nor the names of its contributors may be used
	  to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include <cat/math/BigRTL.hpp>
#include <cat/mem/AlignedAllocator.hpp>
#include <cat/io/Log.hpp>
#include <cstring>
using namespace cat;

BigRTL::BigRTL(int regs, int bits)
{
	_valid = false;

	library_legs = bits / (8 * sizeof(Leg));
    library_regs = regs + BIG_OVERHEAD;

    // Align library memory accesses to a 16-byte boundary
	library_memory = AlignedAllocator::ref()->AcquireArray<Leg>(library_legs * library_regs);
	if (!library_memory)
	{
		CAT_FATAL("BigRTL") << "Unable to allocate leg array for maths";
		return;
	}

	_valid = true;
}

BigRTL::~BigRTL()
{
    if (library_memory)
    {
        // Clear and free memory for registers
        CAT_SECURE_CLR(library_memory, library_legs * library_regs * sizeof(Leg));
        AlignedAllocator::ref()->Delete(library_memory);
    }
}

Leg * CAT_FASTCALL BigRTL::Get(int reg_index)
{
    return &library_memory[library_legs * reg_index];
}

void CAT_FASTCALL BigRTL::Copy(const Leg *in_reg, Leg *out_reg)
{
    memcpy(out_reg, in_reg, library_legs * sizeof(Leg));
}

void CAT_FASTCALL BigRTL::CopyX(Leg in_reg, Leg *out_reg)
{
    // Set low leg to input, zero the rest
    out_reg[0] = in_reg;
    CAT_CLR(&out_reg[1], (library_legs-1) * sizeof(Leg));
}

int CAT_FASTCALL BigRTL::LegsUsed(const Leg *in_reg)
{
    for (int legs = library_legs - 1; legs >= 0; --legs)
        if (in_reg[legs]) return legs + 1;

    return 0;
}

// Strangely enough, including these all in the same source file improves performance
// in Visual Studio by almost 50%, which is odd because MSVC was one of the first
// compilers to support "link time optimization."

#include "rtl/io/Load.cpp"
#include "rtl/io/LoadString.cpp"
#include "rtl/io/Save.cpp"
#include "rtl/addsub/Add.cpp"
#include "rtl/addsub/Add1.cpp"
#include "rtl/addsub/AddX.cpp"
#include "rtl/addsub/Compare.cpp"
#include "rtl/addsub/Double.cpp"
#include "rtl/addsub/DoubleAdd.cpp"
#include "rtl/addsub/Negate.cpp"
#include "rtl/addsub/Shift.cpp"
#include "rtl/addsub/Subtract.cpp"
#include "rtl/addsub/SubtractX.cpp"
#include "rtl/mul/MultiplyX.cpp"
#include "rtl/mul/MultiplyXAdd.cpp"
#include "rtl/mul/Multiply.cpp"
#include "rtl/mul/Square.cpp"
#include "rtl/div/Divide.cpp"
#include "rtl/div/DivideAsm64.cpp"
#include "rtl/div/DivideGeneric.cpp"
#include "rtl/div/ModularInverse.cpp"
#include "rtl/div/MultiplicativeInverse.cpp"
#include "rtl/div/EatTrailingZeroes.cpp"
#include "rtl/div/MulMod.cpp"
