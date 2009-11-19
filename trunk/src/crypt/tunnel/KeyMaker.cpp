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

#include <cat/crypt/tunnel/KeyMaker.hpp>
using namespace cat;

bool KeyMaker::GenerateKeyPair(BigTwistedEdwards *math, FortunaOutput *csprng, u8 *public_key, int public_bytes, u8 *private_key, int private_bytes)
{
    if (!csprng || !math) return false;
	int bits = math->RegBytes() * 8;

    // Validate and accept number of bits
    if (!KeyAgreementCommon::Initialize(bits))
        return false;

    // Verify that inputs are of the correct length
    if (private_bytes != KeyBytes) return false;
    if (public_bytes != KeyBytes*2) return false;

    Leg *b = math->Get(0);
    Leg *B = math->Get(1);

    // Generate private key
	GenerateKey(math, csprng, b);

    // Generate public key
	math->PtMultiply(math->GetGenerator(), b, 0, B);

    // Save key pair and generator point
    math->SaveAffineXY(B, public_key, public_key + KeyBytes);
    math->Save(b, private_key, private_bytes);

    return true;
}