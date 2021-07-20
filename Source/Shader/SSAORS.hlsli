//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#define SSAO_RootSig \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 4), " \
    "DescriptorTable(UAV(u0, numDescriptors = 1))," \
    "DescriptorTable(SRV(t0, numDescriptors = 1))," 
