#ifdef N64VIDEO_C

static struct {
    uint8_t cvg;   // 0 to 8 inclusive (4 bit)
    uint8_t cvbit; // single bit
    uint8_t xoff;  // 4 bit subpixel offset
    uint8_t yoff;  // 4 bit subpixel offset
} cvarray[0x100];

static STRICTINLINE uint32_t
rightcvghex(uint32_t x, uint32_t fmask)
{
    // Mod 8, divided by 2 (rounded to nearest)
    uint32_t covered = ((x & 7) + 1) >> 1;
    return ((0xF0 >> covered) & fmask);
}

static STRICTINLINE uint32_t
leftcvghex(uint32_t x, uint32_t fmask)
{
    // Mod 8, divided by 2 (rounded to nearest)
    uint32_t covered = ((x & 7) + 1) >> 1;
    return ((0x0F >> covered) & fmask);
}

#if 1

static STRICTINLINE void
compute_cvg_unified(struct rdp_state *wstate, int32_t scanline, bool flip)
{
    // TODO move this swap to rasterizer.c ?
    int32_t purgestart = flip ? wstate->span[scanline].rx : wstate->span[scanline].lx;
    int32_t purgeend = flip ? wstate->span[scanline].lx : wstate->span[scanline].rx;

    int length = purgeend - purgestart;
    if (length < 0)
        return; // TODO under what conditions does this occur?

    // Start with full coverage
    memset(&wstate->cvgbuf[purgestart], 0xff, length + 1);

    for (int y_subpx = 0; y_subpx < 4; y_subpx++) {
        int fmask = 0b1010 >> (y_subpx & 1);   // { 0b1010, 0b0101, 0b1010, 0b0101 }
        int maskshift = (y_subpx - 2) & 4;     // {      4,      4,      0,      0 }
        int fmaskshifted = fmask << maskshift; // {   0xA0,   0x50,   0x0A,   0x05 }
        // ~fmaskshifted                          {   0x5F,   0xAF,   0xF5,   0xFA }

        if (!(wstate->span[scanline].invalyscan & (1 << y_subpx))) {
            // Not invalid
            int32_t minorcur = wstate->span[scanline].minorx[y_subpx];
            int32_t majorcur = wstate->span[scanline].majorx[y_subpx];
            int ax = flip ? minorcur : majorcur; // TODO move this swap to rasterizer.c ?
            int bx = flip ? majorcur : minorcur;
            int axint = ax >> 3;
            int bxint = bx >> 3;

            // Mask coverage in the range [purgestart, majorx_int]
            for (int i = purgestart; i <= bxint; i++)
                wstate->cvgbuf[i] &= ~fmaskshifted;

            // Mask coverage in the range [minorx, purgeend]
            for (int i = axint; i <= purgeend; i++)
                wstate->cvgbuf[i] &= ~fmaskshifted;

            if (axint > bxint) {
                // Set partial coverage values on edges
                wstate->cvgbuf[bxint] |= leftcvghex(bx, fmask) << maskshift;
                wstate->cvgbuf[axint] |= rightcvghex(ax, fmask) << maskshift;
            } else if (axint == bxint) {
                // Take the union of the coverage
                wstate->cvgbuf[axint] |= (leftcvghex(bx, fmask) & rightcvghex(ax, fmask)) << maskshift;
            } else {
                // This had better be unreachable, since minorx should be >= majorx
                __builtin_unreachable();
            }
        } else {
            // Invalid, mask coverage in the range [purgestart, purgeend]
            for (int k = purgestart; k <= purgeend; k++)
                wstate->cvgbuf[k] &= ~fmaskshifted;
        }
    }
}

#else

static STRICTINLINE void
compute_cvg_flip(struct rdp_state *wstate, int32_t scanline)
{
    int32_t purgestart = wstate->span[scanline].rx;
    int32_t purgeend = wstate->span[scanline].lx;

    int length = purgeend - purgestart;
    if (length < 0)
        return; // TODO under what conditions does this occur?

    // Start with full coverage
    memset(&wstate->cvgbuf[purgestart], 0xff, length + 1);

    for (int y_subpx = 0; y_subpx < 4; y_subpx++) {
        int fmask = 0b1010 >> (y_subpx & 1);   // { 0b1010, 0b0101, 0b1010, 0b0101 }
        int maskshift = (y_subpx - 2) & 4;     // {      4,      4,      0,      0 }
        int fmaskshifted = fmask << maskshift; // {   0xA0,   0x50,   0x0A,   0x05 }
        // ~fmaskshifted                          {   0x5F,   0xAF,   0xF5,   0xFA }

        if (!(wstate->span[scanline].invalyscan & (1 << y_subpx))) {
            // Not invalid
            int32_t minorcur = wstate->span[scanline].minorx[y_subpx];
            int32_t majorcur = wstate->span[scanline].majorx[y_subpx];
            int32_t minorcurint = minorcur >> 3;
            int32_t majorcurint = majorcur >> 3;

            // Mask coverage in the range [purgestart, majorx_int]
            for (int i = purgestart; i <= majorcurint; i++)
                wstate->cvgbuf[i] &= ~fmaskshifted;
            // Mask coverage in the range [minorx, purgeend]
            for (int i = minorcurint; i <= purgeend; i++)
                wstate->cvgbuf[i] &= ~fmaskshifted;

            if (minorcurint > majorcurint) {
                wstate->cvgbuf[minorcurint] |= rightcvghex(minorcur, fmask) << maskshift;
                wstate->cvgbuf[majorcurint] |= leftcvghex(majorcur, fmask) << maskshift;
            } else if (minorcurint == majorcurint) {
                // Take the union of the coverage
                int32_t samecvg = rightcvghex(minorcur, fmask) & leftcvghex(majorcur, fmask);
                wstate->cvgbuf[majorcurint] |= samecvg << maskshift;
            } else {
                // This had better be unreachable, since minorx should be >= majorx
                __builtin_unreachable();
            }
        } else {
            // Invalid, mask coverage in the range [purgestart, purgeend]
            for (int i = purgestart; i <= purgeend; i++)
                wstate->cvgbuf[i] &= ~fmaskshifted;
        }
    }
}

static STRICTINLINE void
compute_cvg_noflip(struct rdp_state *wstate, int32_t scanline)
{
    int32_t purgestart = wstate->span[scanline].lx;
    int32_t purgeend = wstate->span[scanline].rx;

    int length = purgeend - purgestart;
    if (length < 0)
        return; // TODO under what conditions does this occur?

    // Start with full coverage
    memset(&wstate->cvgbuf[purgestart], 0xff, length + 1);

    for (int y_subpx = 0; y_subpx < 4; y_subpx++) {
        int fmask = 0b1010 >> (y_subpx & 1);   // { 0b1010, 0b0101, 0b1010, 0b0101 }
        int maskshift = (y_subpx - 2) & 4;     // {      4,      4,      0,      0 }
        int fmaskshifted = fmask << maskshift; // {   0xA0,   0x50,   0x0A,   0x05 }
        //     ~fmaskshifted                      {   0x5F,   0xAF,   0xF5,   0xFA }

        if (!(wstate->span[scanline].invalyscan & (1 << y_subpx))) {
            // Not invalid
            int32_t minorcur = wstate->span[scanline].minorx[y_subpx];
            int32_t majorcur = wstate->span[scanline].majorx[y_subpx];
            int32_t minorcurint = minorcur >> 3;
            int32_t majorcurint = majorcur >> 3;

            // Mask coverage in the range [purgestart, majorx_int]
            for (int k = purgestart; k <= minorcurint; k++)
                wstate->cvgbuf[k] &= ~fmaskshifted;
            // Mask coverage in the range [minorx, purgeend]
            for (int k = majorcurint; k <= purgeend; k++)
                wstate->cvgbuf[k] &= ~fmaskshifted;

            if (majorcurint > minorcurint) {
                wstate->cvgbuf[minorcurint] |= leftcvghex(minorcur, fmask) << maskshift;
                wstate->cvgbuf[majorcurint] |= rightcvghex(majorcur, fmask) << maskshift;
            } else if (minorcurint == majorcurint) {
                // Take the union of the coverage
                int32_t samecvg = leftcvghex(minorcur, fmask) & rightcvghex(majorcur, fmask);
                wstate->cvgbuf[majorcurint] |= samecvg << maskshift;
            } else {
                // This had better be unreachable, since minorx should be >= majorx
                __builtin_unreachable();
            }
        } else {
            // Invalid, mask coverage in the range [purgestart, purgeend]
            for (int k = purgestart; k <= purgeend; k++)
                wstate->cvgbuf[k] &= ~fmaskshifted;
        }
    }
}

#endif

#define CVG_CLAMP 0
#define CVG_WRAP  1
#define CVG_ZAP   2
#define CVG_SAVE  3

static STRICTINLINE int
finalize_spanalpha(int cvg_dest, uint32_t blend_en, uint32_t curpixel_cvg, uint32_t curpixel_memcvg)
{
    int finalcvg;

    switch (cvg_dest) {
        case_no_default;

        case CVG_CLAMP:
            // If blending is enabled, add memory cvg first
            if (blend_en)
                finalcvg = curpixel_cvg + curpixel_memcvg;
            else
                finalcvg = curpixel_cvg - 1;

            // Perform clamp
            if (finalcvg & 8)
                finalcvg = 7;
            else
                finalcvg &= 7;
            break;

        case CVG_WRAP:
            // Wrap, keep only low order bits
            finalcvg = (curpixel_cvg + curpixel_memcvg) & 7;
            break;

        case CVG_ZAP:
            // Force full coverage
            finalcvg = 7;
            break;

        case CVG_SAVE:
            // Leave coverage unchanged
            finalcvg = curpixel_memcvg;
            break;
    }

    return finalcvg;
}

static STRICTINLINE void
lookup_cvmask_derivatives(uint8_t mask, uint8_t *offx, uint8_t *offy, uint32_t *curpixel_cvg, uint32_t *curpixel_cvbit)
{
    *curpixel_cvg = cvarray[mask].cvg;
    *curpixel_cvbit = cvarray[mask].cvbit;
    *offx = cvarray[mask].xoff;
    *offy = cvarray[mask].yoff;
}

static void
coverage_init_lut(void)
{
    static const uint8_t lz_count[16] = {
        // Counts the number of leading 0s in a 4-bit number,
        // 0 is defined to be 0
        [0b0000] = 0,
        [0b0001] = 3,
        [0b0010] = 2,
        [0b0011] = 2,
        [0b0100] = 1,
        [0b0101] = 1,
        [0b0110] = 1,
        [0b0111] = 1,
        [0b1000] = 0,
        [0b1001] = 0,
        [0b1010] = 0,
        [0b1011] = 0,
        [0b1100] = 0,
        [0b1101] = 0,
        [0b1110] = 0,
        [0b1111] = 0,
    };
    static const uint8_t tz_count[16] = {
        // Counts the number of trailing 0s in a 4-bit number,
        // 0 is defined to be 0
        [0b0000] = 0,
        [0b0001] = 0,
        [0b0010] = 1,
        [0b0011] = 0,
        [0b0100] = 2,
        [0b0101] = 0,
        [0b0110] = 1,
        [0b0111] = 0,
        [0b1000] = 3,
        [0b1001] = 0,
        [0b1010] = 1,
        [0b1011] = 0,
        [0b1100] = 2,
        [0b1101] = 0,
        [0b1110] = 1,
        [0b1111] = 0,
    };

    for (unsigned i = 0; i < 256; i++) {
        // Coverage masks are arranged as
        //    1 0 1 0
        //    0 1 0 1
        //    1 0 1 0
        //    0 1 0 1
        uint8_t c = i;
        uint16_t mask = (c & 0b00000101) | ((c & 0b01011010) << 4) | ((c & 0b10100000) << 8);

        // Fast lookup of msbit for non-AA pixel rejection
        cvarray[i].cvbit = (c >> 7) & 1;

        // Population count gives the coverage value
        // (note that c has the same amount of bits set as mask does, so popcount(c) == popcount(mask))
        cvarray[i].cvg = __builtin_popcount(c);
        // for (unsigned j = 0; j < 8; j++)
        //     cvarray[i].cvg += ((c >> j) & 1);

        // Prepare subpixel offsets for attribute correction

        // This constructs a mask based on how many subpixel rows were touched
        // 0b1111 indicates all subpixel rows are touched, 0b1000 indicates only the
        // lowest row was touched, 0b0001 indicates only the highest row was touched
        uint8_t masky = (((mask & 0b1111000000000000) != 0) << 0)
                      | (((mask & 0b0000111100000000) != 0) << 1)
                      | (((mask & 0b0000000011110000) != 0) << 2)
                      | (((mask & 0b0000000000001111) != 0) << 3);

        // Trailing zero count gives the subpixel y offset
        uint8_t offy = tz_count[masky];

        // This extracts the row identified to contain the first covered y subpixel,
        // then the row contents is further shifted in x inversely proportional to
        // the row number
        uint8_t maskx = (mask & (0xF000 >> (offy * 4))) >> ((offy ^ 0b11) * 4);

        // Leading zero count gives the subpixel x offset
        uint8_t offx = lz_count[maskx];

        cvarray[i].xoff = offx; // Result in the range [0,3]
        cvarray[i].yoff = offy; // Result in the range [0,3]
    }
}

#endif // N64VIDEO_C
