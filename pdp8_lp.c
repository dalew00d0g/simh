/* pdp8_lp.c: PDP-8 line printer simulator

   Copyright (c) 1993-1999, Robert M Supnik

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert M Supnik shall not
   be used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   lpt		line printer
*/

#include "pdp8_defs.h"

extern int32 int_req, dev_done, dev_enable, stop_inst;
int32 lpt_err = 0;					/* error flag */
int32 lpt_stopioe = 0;					/* stop on error */
t_stat lpt_svc (UNIT *uptr);
t_stat lpt_reset (DEVICE *dptr);
t_stat lpt_attach (UNIT *uptr, char *cptr);
t_stat lpt_detach (UNIT *uptr);
extern t_stat sim_activate (UNIT *uptr, int32 delay);
extern t_stat sim_cancel (UNIT *uptr);

/* LPT data structures

   lpt_dev	LPT device descriptor
   lpt_unit	LPT unit descriptor
   lpt_reg	LPT register list
*/

UNIT lpt_unit = {
	UDATA (&lpt_svc, UNIT_SEQ+UNIT_ATTABLE, 0), SERIAL_OUT_WAIT };

REG lpt_reg[] = {
	{ ORDATA (BUF, lpt_unit.buf, 8) },
	{ FLDATA (ERR, lpt_err, 0) },
	{ FLDATA (DONE, dev_done, INT_V_LPT) },
	{ FLDATA (ENABLE, dev_enable, INT_V_LPT) },
	{ FLDATA (INT, int_req, INT_V_LPT) },
	{ DRDATA (POS, lpt_unit.pos, 31), PV_LEFT },
	{ DRDATA (TIME, lpt_unit.wait, 24), PV_LEFT },
	{ FLDATA (STOP_IOE, lpt_stopioe, 0) },
	{ NULL }  };

DEVICE lpt_dev = {
	"LPT", &lpt_unit, lpt_reg, NULL,
	1, 10, 31, 1, 8, 8,
	NULL, NULL, &lpt_reset,
	NULL, &lpt_attach, &lpt_detach };

/* IOT routine */

int32 lpt (int32 pulse, int32 AC)
{
switch (pulse) {					/* decode IR<9:11> */
case 1:							/* PSKF */
	return (dev_done & INT_LPT)? IOT_SKP + AC: AC;
case 2:							/* PCLF */
	dev_done = dev_done & ~INT_LPT;			/* clear flag */
	int_req = int_req & ~INT_LPT;			/* clear int req */
	return AC;
case 3:							/* PSKE */
	return (lpt_err)? IOT_SKP + AC: AC;
case 6:							/* PCLF!PSTB */
	dev_done = dev_done & ~INT_LPT;			/* clear flag */
	int_req = int_req & ~INT_LPT;			/* clear int req */
case 4:							/* PSTB */
	lpt_unit.buf = AC & 0177;			/* load buffer */
	if ((lpt_unit.buf == 015) || (lpt_unit.buf == 014) ||
	    (lpt_unit.buf == 012)) {
		sim_activate (&lpt_unit, lpt_unit.wait);
		return AC;  }
	return (lpt_svc (&lpt_unit) << IOT_V_REASON) + AC;
case 5:							/* PSIE */
	dev_enable = dev_enable | INT_LPT;		/* set enable */
	int_req = INT_UPDATE;				/* update interrupts */
	return AC;
case 7:							/* PCIE */
	dev_enable = dev_enable & ~INT_LPT;		/* clear enable */
	int_req = int_req & ~INT_LPT;			/* clear int req */
	return AC;
default:
	return (stop_inst << IOT_V_REASON) + AC;  }	/* end switch */
}

/* Unit service */

t_stat lpt_svc (UNIT *uptr)
{
dev_done = dev_done | INT_LPT;				/* set done */
int_req = INT_UPDATE;					/* update interrupts */
if ((lpt_unit.flags & UNIT_ATT) == 0) {
	lpt_err = 1;
	return IORETURN (lpt_stopioe, SCPE_UNATT);  }
if (putc (lpt_unit.buf, lpt_unit.fileref) == EOF) {
	perror ("LPT I/O error");
	clearerr (lpt_unit.fileref);
	return SCPE_IOERR;  }
lpt_unit.pos = ftell (lpt_unit.fileref);
return SCPE_OK;
}

/* Reset routine */

t_stat lpt_reset (DEVICE *dptr)
{
lpt_unit.buf = 0;
dev_done = dev_done & ~INT_LPT;				/* clear done, int */
int_req = int_req & ~INT_LPT;
dev_enable = dev_enable | INT_LPT;			/* set enable */
lpt_err = (lpt_unit.flags & UNIT_ATT) == 0;
sim_cancel (&lpt_unit);					/* deactivate unit */
return SCPE_OK;
}

/* Attach routine */

t_stat lpt_attach (UNIT *uptr, char *cptr)
{
t_stat reason;

reason = attach_unit (uptr, cptr);
lpt_err = (lpt_unit.flags & UNIT_ATT) == 0;
return reason;
}

/* Detach routine */

t_stat lpt_detach (UNIT *uptr)
{
lpt_err = 1;
return detach_unit (uptr);
}
