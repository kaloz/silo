/* Prom ranges
   
   Copyright (C) 1996 David Miller
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

#include <silo.h>

struct linux_prom_ranges promlib_obio_ranges[PROMREG_MAX];
int num_obio_ranges;

/* Adjust register values based upon the ranges parameters. */
void prom_adjust_regs (struct linux_prom_registers *regp, int nregs,
		       struct linux_prom_ranges *rangep, int nranges)
{
    int regc, rngc;

    for (regc = 0; regc < nregs; regc++) {
	for (rngc = 0; rngc < nranges; rngc++)
	    if (regp[regc].which_io == rangep[rngc].ot_child_space)
		break;		/* Found it */
	if (rngc == nranges)	/* oops */
	    printf ("adjust_regs: Could not find range with matching bus type...\n");
	regp[regc].which_io = rangep[rngc].ot_parent_space;
	regp[regc].phys_addr += rangep[rngc].ot_parent_base;
    }
}

/* Apply probed obio ranges to registers passed, if no ranges return. */
void prom_apply_obio_ranges (struct linux_prom_registers *regs, int nregs)
{
    if (!num_obio_ranges)
	return;
    prom_adjust_regs (regs, nregs, promlib_obio_ranges, num_obio_ranges);
}

void prom_ranges_init (void)
{
    int node, obio_node;
    int success;

    num_obio_ranges = 0;
    
    /* Check for obio and sbus ranges. */
    node = prom_getchild (prom_root_node);
    obio_node = prom_searchsiblings (node, "obio");
    if (obio_node) {
	success = prom_getproperty (obio_node, "ranges",
				    (char *) promlib_obio_ranges,
				    sizeof (promlib_obio_ranges));
	if (success != -1)
	    num_obio_ranges = (success / sizeof (struct linux_prom_ranges));
    }
}
