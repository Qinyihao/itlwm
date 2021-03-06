/*
* Copyright (C) 2020  钟先耀
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
/*    $OpenBSD: if_iwm.c,v 1.307 2020/04/09 21:36:50 stsp Exp $    */

/*
 * Copyright (c) 2014, 2016 genua gmbh <info@genua.de>
 *   Author: Stefan Sperling <stsp@openbsd.org>
 * Copyright (c) 2014 Fixup Software Ltd.
 * Copyright (c) 2017 Stefan Sperling <stsp@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*-
 * Based on BSD-licensed source modules in the Linux iwlwifi driver,
 * which were used as the reference documentation for this implementation.
 *
 ***********************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2013 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016 Intel Deutschland GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2013 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016 Intel Deutschland GmbH
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2007-2010 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "itlwm.hpp"
#include <IOKit/IODMACommand.h>

uint32_t itlwm::
iwm_read_prph(struct iwm_softc *sc, uint32_t addr)
{
    iwm_nic_assert_locked(sc);
    IWM_WRITE(sc,
        IWM_HBUS_TARG_PRPH_RADDR, ((addr & 0x000fffff) | (3 << 24)));
    IWM_BARRIER_READ_WRITE(sc);
    return IWM_READ(sc, IWM_HBUS_TARG_PRPH_RDAT);
}

void itlwm::
iwm_write_prph(struct iwm_softc *sc, uint32_t addr, uint32_t val)
{
    iwm_nic_assert_locked(sc);
    IWM_WRITE(sc,
        IWM_HBUS_TARG_PRPH_WADDR, ((addr & 0x000fffff) | (3 << 24)));
    IWM_BARRIER_WRITE(sc);
    IWM_WRITE(sc, IWM_HBUS_TARG_PRPH_WDAT, val);
}

void itlwm::
iwm_write_prph64(struct iwm_softc *sc, uint64_t addr, uint64_t val)
{
    iwm_write_prph(sc, (uint32_t)addr, val & 0xffffffff);
    iwm_write_prph(sc, (uint32_t)addr + 4, val >> 32);
}

int itlwm::
iwm_read_mem(struct iwm_softc *sc, uint32_t addr, void *buf, int dwords)
{
    int offs, err = 0;
    uint32_t *vals = (uint32_t*)buf;

    if (iwm_nic_lock(sc)) {
        IWM_WRITE(sc, IWM_HBUS_TARG_MEM_RADDR, addr);
        for (offs = 0; offs < dwords; offs++)
            vals[offs] = IWM_READ(sc, IWM_HBUS_TARG_MEM_RDAT);
        iwm_nic_unlock(sc);
    } else {
        err = EBUSY;
    }
    return err;
}

int itlwm::
iwm_write_mem(struct iwm_softc *sc, uint32_t addr, const void *buf, int dwords)
{
    int offs;
    const uint32_t *vals = (const uint32_t*)buf;

    if (iwm_nic_lock(sc)) {
        IWM_WRITE(sc, IWM_HBUS_TARG_MEM_WADDR, addr);
        /* WADDR auto-increments */
        for (offs = 0; offs < dwords; offs++) {
            uint32_t val = vals ? vals[offs] : 0;
            IWM_WRITE(sc, IWM_HBUS_TARG_MEM_WDAT, val);
        }
        iwm_nic_unlock(sc);
    } else {
        return EBUSY;
    }
    return 0;
}

int itlwm::
iwm_write_mem32(struct iwm_softc *sc, uint32_t addr, uint32_t val)
{
    return iwm_write_mem(sc, addr, &val, 1);
}

int itlwm::
iwm_poll_bit(struct iwm_softc *sc, int reg, uint32_t bits, uint32_t mask,
    int timo)
{
    for (;;) {
        if ((IWM_READ(sc, reg) & mask) == (bits & mask)) {
            return 1;
        }
        if (timo < 10) {
            return 0;
        }
        timo -= 10;
        DELAY(10);
    }
}

int itlwm::
iwm_nic_lock(struct iwm_softc *sc)
{
    if (sc->sc_nic_locks > 0) {
        iwm_nic_assert_locked(sc);
        sc->sc_nic_locks++;
        return 1; /* already locked */
    }

    IWM_SETBITS(sc, IWM_CSR_GP_CNTRL,
        IWM_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);

    if (sc->sc_device_family == IWM_DEVICE_FAMILY_8000)
        DELAY(2);

    if (iwm_poll_bit(sc, IWM_CSR_GP_CNTRL,
        IWM_CSR_GP_CNTRL_REG_VAL_MAC_ACCESS_EN,
        IWM_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY
         | IWM_CSR_GP_CNTRL_REG_FLAG_GOING_TO_SLEEP, 150000)) {
        sc->sc_nic_locks++;
        return 1;
    }

    XYLog("%s: acquiring device failed\n", DEVNAME(sc));
    return 0;
}

void itlwm::
iwm_nic_assert_locked(struct iwm_softc *sc)
{
    uint32_t reg = IWM_READ(sc, IWM_CSR_GP_CNTRL);
    if ((reg & IWM_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY) == 0)
        panic("%s: mac clock not ready", DEVNAME(sc));
    if (reg & IWM_CSR_GP_CNTRL_REG_FLAG_GOING_TO_SLEEP)
        panic("%s: mac gone to sleep", DEVNAME(sc));
    if (sc->sc_nic_locks <= 0)
        panic("%s: nic locks counter %d", DEVNAME(sc), sc->sc_nic_locks);
}

void itlwm::
iwm_nic_unlock(struct iwm_softc *sc)
{
    if (sc->sc_nic_locks > 0) {
        if (--sc->sc_nic_locks == 0)
            IWM_CLRBITS(sc, IWM_CSR_GP_CNTRL,
                IWM_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
    } else
        XYLog("%s: NIC already unlocked\n", DEVNAME(sc));
}

void itlwm::
iwm_set_bits_mask_prph(struct iwm_softc *sc, uint32_t reg, uint32_t bits,
    uint32_t mask)
{
    uint32_t val;

    /* XXX: no error path? */
    if (iwm_nic_lock(sc)) {
        val = iwm_read_prph(sc, reg) & mask;
        val |= bits;
        iwm_write_prph(sc, reg, val);
        iwm_nic_unlock(sc);
    }
}

void itlwm::
iwm_set_bits_prph(struct iwm_softc *sc, uint32_t reg, uint32_t bits)
{
    iwm_set_bits_mask_prph(sc, reg, bits, ~0);
}

void itlwm::
iwm_clear_bits_prph(struct iwm_softc *sc, uint32_t reg, uint32_t bits)
{
    iwm_set_bits_mask_prph(sc, reg, 0, ~bits);
}

IOBufferMemoryDescriptor* allocDmaMemory
( size_t size, int alignment, void** vaddr, uint64_t* paddr )
{
    size_t        reqsize;
    uint64_t    phymask;
    int        i;
    
    
    if (alignment <= PAGE_SIZE) {
        reqsize = size;
        phymask = 0x00000000ffffffffull & (~(alignment - 1));
    } else {
        reqsize = size + alignment;
        phymask = 0x00000000fffff000ull; /* page-aligned */
    }
    
    IOBufferMemoryDescriptor* mem = 0;
    mem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut | kIOMapInhibitCache,
                                   reqsize, phymask);
    if (!mem) {
        XYLog("Could not allocate DMA memory\n");
        return 0;
    }
    mem->prepare();
    *paddr = mem->getPhysicalAddress();
    *vaddr = mem->getBytesNoCopy();
    
    /*
     * Check the alignment and increment by 4096 until we get the
     * requested alignment. Fail if can't obtain the alignment
     * we requested.
     */
    if ((*paddr & (alignment - 1)) != 0) {
        for (i = 0; i < alignment / 4096; i++) {
            if ((*paddr & (alignment - 1 )) == 0)
                break;
            *paddr += 4096;
            *vaddr = ((uint8_t*) *vaddr) + 4096;
        }
        if (i == alignment / 4096) {
            XYLog("Memory alloc alignment requirement %d was not satisfied\n", alignment);
            mem->complete();
            mem->release();
            return 0;
        }
    }
    return mem;
}

IOBufferMemoryDescriptor* allocDmaMemory2(size_t size, int alignment, void** vaddr, uint64_t* paddr)
{
    IOBufferMemoryDescriptor *bmd;
    IODMACommand::Segment64 seg;
    UInt64 ofs = 0;
    UInt32 numSegs = 1;
    int        i;
    
    bmd = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous | kIOMapInhibitCache, size, DMA_BIT_MASK(36));
    
    IODMACommand *cmd = IODMACommand::withSpecification(kIODMACommandOutputHost64, 64, 0, IODMACommand::kMapped, 0, 1);
    cmd->setMemoryDescriptor(bmd);
    cmd->prepare();
    
    if (cmd->gen64IOVMSegments(&ofs, &seg, &numSegs) != kIOReturnSuccess) {
        cmd->complete();
        cmd->release();
        cmd = NULL;
        bmd->complete();
        bmd->release();
        bmd = NULL;
        return NULL;
    }
    *paddr = seg.fIOVMAddr;
    *vaddr = bmd->getBytesNoCopy();
    return bmd;
}

void itlwm::
iwm_dma_contig_free(struct iwm_dma_info *dma)
{
    if (dma == NULL)
        return;
    if (dma->vaddr == NULL)
        return;
    dma->buffer->complete();
    dma->buffer->release();
    dma->buffer = 0;
    dma->vaddr = 0;
    dma->paddr = 0;
    return;
}

int itlwm::
iwm_dma_contig_alloc(bus_dma_tag_t tag, struct iwm_dma_info *dma, void **kvap,
             bus_size_t size, bus_size_t alignment)
{
    dma->buffer = allocDmaMemory2((size_t)size, (int)alignment, (void**)&dma->vaddr, &dma->paddr);
    if (dma->buffer == NULL)
        return 1;
    
    dma->size = size;
    if (kvap != NULL)
        *kvap = dma->vaddr;
    
    return 0;
}
