/**
 * GZIP decompressor
 * This can be useful to decompress at run-time in memory.
 *
 * Resources:
 * [1] https://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art001
 * [2] https://www.ietf.org/rfc/rfc1951.txt
 * [3] https://www.ietf.org/rfc/rfc1952.txt
 *
 * Usage:
 *
 *    void *in, *out;
 *    unsigned int insize, outsize;
 *    int result;
 *    void *in = read_entire_file("myfile.bin.gz", &insize);
 *    outsize = gzdecsize(in, insize);
 *    out = malloc(outsize);
 *    result = gzdec(in, insize, out, outsize);
 *    if(result == GZ_OK)
 *    {
 *      write_entire_file("myfile.bin", out, outsize);
 *    }
 *    else
 *    {
 *      printf("Cannot decompress file");
 *    }
 *
 * TODO:
 * [ ] Test BTYPE=00 and BTYPE=01
 */

#ifndef GZDEC_H
#define GZDEC_H

enum gz_result
{
    GZ_OK,
    GZ_INVMAGIC,
    GZ_INVCMETHOD,
    GZ_INVFILE,
    GZ_NOSPACE
};

unsigned int gzdecsize(void *in, unsigned int insize);
int gzdec(void *in, unsigned int insize, void *out, unsigned int outsize);

#ifdef GZDEC_IMPLEMENTATION
#ifndef GZDEC_IMPLEMENTED
#define GZDEC_IMPLEMENTED

typedef struct
gz_bstream
{
    unsigned char *src;
    unsigned char *srcend;
    unsigned char *ptr;
    unsigned char buf;
    /* current bit position within buf, 8 is MSB */
    unsigned char mask;
    int end;
} gz_bstream;

typedef struct
gz_huffn
{
    int code; /* -1 0=> non-leaf */
    struct gz_huffn *zero;
    struct gz_huffn *one;
} gz_huffn;

static unsigned int
gz_nextbit(gz_bstream *stream)
{
    unsigned int bit = 0;

    if(stream->end)
    {
        return(0);
    }

    bit = (stream->buf & stream->mask) ? 1 : 0;
    stream->mask <<= 1;
    if(!stream->mask)
    {
        stream->mask = 1;

        if(stream->ptr < stream->srcend)
        {
            stream->buf = *stream->ptr++;
        }
        else
        {
            stream->end = 1;
        }
    }

    return(bit);
}

static unsigned int
gz_readbits(gz_bstream *stream, int count)
{
    int bitsval, i, bit;

    bitsval = 0;
    for(i = 0;
        i < count;
        ++i)
    {
        bit = gz_nextbit(stream);
        bitsval |= (bit << i);
    }

    return(bitsval);
}

void
gz_memset(void *dst, int val, unsigned int count)
{
    unsigned char *p;
    unsigned int i;

    p = (unsigned char *)dst;
    for(i = 0;
        i < count;
        ++i)
    {
        *p++ = (unsigned char)(val & 0xff);
    }
}

int
gz_huffdec(gz_bstream *stream, gz_huffn *ht)
{
    gz_huffn *n;

    n = ht;
    while(n && n->code < 0)
    {
        if(gz_readbits(stream, 1))
        {
            n = n->one;
        }
        else
        {
            n = n->zero;
        }

        if(n && n->code >= 0)
        {
            return(n->code);
        }
    }

    return(-1);
}

typedef struct
gz_tnode
{
    unsigned int blen;
    unsigned int code;
} gz_tnode;

typedef struct
gz_range
{
    int end;
    int blen;
} gz_range;

int
gz_torange(
    unsigned int *cl, unsigned int count,
    gz_range *ranges, unsigned int rmax)
{
    unsigned int i, j;

    for(j = 0;
        j < rmax;
        ++j)
    {
        ranges[j].end = 0;
        ranges[j].blen = 0;
    }

    j = 0;
    for(i = 0;
        i < count;
        ++i)
    {
        if((i > 0) && (cl[i] != cl[i-1]))
        {
            ++j;
        }

        if(j >= rmax)
        {
            return(-1);
        }

        ranges[j].end = i;
        ranges[j].blen = cl[i];
    }

    return(j + 1);
}

#define GZ_LL_MAX 288
#define GZ_DIST_MAX 32
#define GZ_CLEN_MAX 19

#define GZ_RANGE_MAX GZ_LL_MAX
#define GZ_BL_COUNT_MAX 30
#define GZ_NXTCODE_MAX GZ_BL_COUNT_MAX
#define GZ_TREE_MAX GZ_LL_MAX

gz_range gz_range_[GZ_RANGE_MAX];
int gz_blcount_[GZ_BL_COUNT_MAX];
int gz_nxtcode_[GZ_NXTCODE_MAX];
gz_tnode gz_tree_[GZ_TREE_MAX];

int
gz_buildht(
    unsigned int *cl, unsigned int count,
    gz_huffn *ht, unsigned int htmax)
{
    int *blcount, *nxtcode;
    gz_tnode *tree;
    int i, bits;
    int code;
    gz_range *range;
    unsigned int maxblen, actrange;
    int rcount;
    unsigned int huffcount;
    gz_huffn *node;

    if(count > GZ_LL_MAX)
    {
        return(0);
    }

    range = gz_range_;
    rcount = gz_torange(cl, count, range, count);
    if(rcount < 0)
    {
        return(0);
    }

    maxblen = 0;
    for(i = 0;
        i < rcount;
        ++i)
    {
        if(range[i].blen > (int)maxblen)
        {
            maxblen = range[i].blen;
        }
    }

    if(maxblen + 1 > GZ_BL_COUNT_MAX)
    {
        return(0);
    }
    blcount = gz_blcount_;

    if(maxblen + 1 > GZ_NXTCODE_MAX)
    {
        return(0);
    }
    nxtcode = gz_nxtcode_;

    if(range[rcount - 1].end + 1 > GZ_TREE_MAX)
    {
        return(0);
    }
    tree = gz_tree_;

    if(!blcount || !nxtcode || !tree)
    {
        return(0);
    }

    gz_memset(blcount, 0, sizeof(int) * (maxblen + 1));
    for(i = 0;
        i < rcount;
        ++i)
    {
        blcount[range[i].blen] +=
            range[i].end - ((i > 0) ? range[i-1].end : -1);
    }

    gz_memset(nxtcode, 0, sizeof(int) * (maxblen + 1));
    code = 0;
    for(bits = 1;
        bits <= (int)maxblen;
        ++bits)
    {
        code = (code + blcount[bits-1]) << 1;
        if(blcount[bits])
        {
            nxtcode[bits] = code;
        }
    }

    gz_memset(tree, 0, sizeof(gz_tnode) * (range[rcount - 1].end + 1));
    actrange = 0;
    for(i = 0;
        i <= range[rcount - 1].end;
        ++i)
    {
        if(i > range[actrange].end)
        {
            ++actrange;
        }

        if(range[actrange].blen)
        {
            tree[i].blen = range[actrange].blen;
            if(tree[i].blen != 0)
            {
                tree[i].code = nxtcode[tree[i].blen];
                nxtcode[tree[i].blen] += 1;
            }
        }
    }

    /* Get number of huffman nodes */
    if(!ht)
    {
        return(0);
    }

    gz_memset(ht, 0, sizeof(gz_huffn) * htmax); 

    huffcount = 0;
    node = &(ht[huffcount++]);
    node->code = -1;
    for(i = 0;
        i <= range[rcount - 1].end;
        ++i)
    {
        node = ht;
        if(tree[i].blen)
        {

            for(bits = tree[i].blen;
                bits > 0;
                --bits)
            {
                if(tree[i].code & (1 << (bits - 1)))
                {
                    if(!node->one)
                    {
                        if(huffcount >= htmax)
                        {
                            return(0);
                        }
                        node->one = &(ht[huffcount++]);
                        node->one->code = -1;
                    }
                    node = node->one;
                }
                else
                {
                    if(!node->zero)
                    {
                        if(huffcount >= htmax)
                        {
                            return(0);
                        }
                        node->zero = &(ht[huffcount++]);
                        node->zero->code = -1;
                    }
                    node = node->zero;
                }
            }
            if(node->code != -1)
            {
                return(0);
            }
            node->code = i;
        }
    }

    return(1);
}

int
gz_gethufft(
    gz_bstream *stream,
    unsigned int *lengths,
    unsigned int count,
    unsigned int maxcount,
    gz_huffn *htclen,
    gz_huffn *ht,
    unsigned int htmax)
{
    unsigned int i, val, rep;
    int sym;

    for(i = 0;
        i < maxcount;
        ++i)
    {
        lengths[i] = 0;
    }

    i = 0;
    while(i < count)
    {
        rep = 0;
        sym = gz_huffdec(stream, htclen);
        if(sym < 0 || sym > 18)
        {
            return(0);
        }

        if(sym >= 0 && sym <= 15)
        {
            val = (unsigned int)sym;
            rep = 1;
        }
        else if(sym == 16)
        {
            if(i == 0)
            {
                return(0);
            }
            val = lengths[i - 1];
            rep = 3 + gz_readbits(stream, 2);
        }
        else if(sym == 17)
        {
            val = 0;
            rep = 3 + gz_readbits(stream, 3);
        }
        else if(sym == 18)
        {
            val = 0;
            rep = 11 + gz_readbits(stream, 7);
        }
        else
        {
            return(0);
        }

        while(rep > 0)
        {
            lengths[i++] = val;
            rep -= 1;
        }
    }

    if(i != count)
    {
        return(0);
    }

    return(gz_buildht(lengths, maxcount, ht, htmax));
}

/**
       Extra               Extra               Extra
  Code Bits Length(s) Code Bits Lengths   Code Bits Length(s)
  ---- ---- ------     ---- ---- -------   ---- ---- -------
   257   0     3       267   1   15,16     277   4   67-82
   258   0     4       268   1   17,18     278   4   83-98
   259   0     5       269   2   19-22     279   4   99-114
   260   0     6       270   2   23-26     280   4  115-130
   261   0     7       271   2   27-30     281   5  131-162
   262   0     8       272   2   31-34     282   5  163-194
   263   0     9       273   3   35-42     283   5  195-226
   264   0    10       274   3   43-50     284   5  227-257
   265   1  11,12      275   3   51-58     285   0    258
   266   1  13,14      276   3   59-66
*/

int gz_lentable[] = {
    11, 13, 15, 17, 19, 23, 27,
    31, 35, 43, 51, 59, 67, 83,
    99, 115, 131, 163, 195, 227
};

int
gz_getlen(int code, gz_bstream *stream)
{
    int extra;

    if(code < 257 || code > 285)
    {
        return(-1);
    }

    if (code == 285)
    {
        return(258);
    }

    if(code < 265)
    {
        return(code - 254);
    }

    extra = (int)gz_readbits(stream, (code - 261) / 4);
    return(extra + gz_lentable[code - 265]);
}

/**
        Extra           Extra               Extra
   Code Bits Dist  Code Bits   Dist     Code Bits Distance
   ---- ---- ----  ---- ----  ------    ---- ---- --------
     0   0    1     10   4     33-48    20    9   1025-1536
     1   0    2     11   4     49-64    21    9   1537-2048
     2   0    3     12   5     65-96    22   10   2049-3072
     3   0    4     13   5     97-128   23   10   3073-4096
     4   1   5,6    14   6    129-192   24   11   4097-6144
     5   1   7,8    15   6    193-256   25   11   6145-8192
     6   2   9-12   16   7    257-384   26   12  8193-12288
     7   2  13-16   17   7    385-512   27   12 12289-16384
     8   3  17-24   18   8    513-768   28   13 16385-24576
     9   3  25-32   19   8   769-1024   29   13 24577-32768
*/

int gz_disttable[] = {
    4, 6, 8, 12, 16, 24, 32, 48,
    64, 96, 128, 192, 256, 384,
    512, 768, 1024, 1536, 2048,
    3072, 4096, 6144, 8192,
    12288, 16384, 24576
};

int
gz_getdist(int code, gz_bstream *stream)
{
    int extra;

    if(code < 0 || code > 29)
    {
        return(-1);
    }

    if(code < 4)
    {
        return(code + 1);
    }

    extra = gz_readbits(stream, (code - 2) / 2);
    return(extra + gz_disttable[code - 4] + 1);
}

#define GZ_HTLL_MAX ((GZ_LL_MAX)*2 - 1)
#define GZ_HTDIST_MAX ((GZ_DIST_MAX)*2 - 1)
#define GZ_HTCLEN_MAX ((GZ_CLEN_MAX)*2 - 1)

gz_huffn gz_htll_[GZ_HTLL_MAX];
gz_huffn gz_htdist_[GZ_HTDIST_MAX];
gz_huffn gz_htclen_[GZ_HTCLEN_MAX];

int
gzdec(
    void *in, unsigned int insize,
    void *out, unsigned int outsize)
{
#define FTEXT 0x01
#define FHCRC 0x02
#define FEXTRA 0x04
#define FNAME 0x08
#define FCOMMENT 0x10
#define MAXCLEN GZ_CLEN_MAX
#define LLLEN GZ_LL_MAX
#define DISTLEN GZ_DIST_MAX

#define EMIT(b)\
    if(outp >= outend)\
    {\
        return(GZ_INVFILE);\
    }\
    *outp++ = (unsigned char)((b) & 0xff);

    gz_bstream ins = {0};
    unsigned char magic[2];
    unsigned int cm, flags, xlen;
    unsigned int islast, btype;
    unsigned int hlit, hdist, hclen;
    unsigned int i;
    int sym, dist, len;
    int todec;

    unsigned int b0len, b0nlen;

    unsigned int arrclen[MAXCLEN] = {0};
    unsigned int clenord[MAXCLEN] = {
        16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
    };
    gz_huffn *htclen;

    unsigned int arrll[LLLEN] = {0};
    unsigned int arrdist[DISTLEN] = {0};
    gz_huffn *htll, *htdist;

    unsigned char *outp, *outend, *backp;

    outp = (unsigned char *)out;
    outend = outp + outsize;

    if(!in || insize < 18)
    {
        return(GZ_INVFILE);
    }

    i = gzdecsize(in, insize);
    if(i == 0)
    {
        return(GZ_INVFILE);
    }

    if(outsize < i)
    {
        return(GZ_NOSPACE);
    }

    htll = gz_htll_;
    htdist = gz_htdist_;
    htclen = gz_htclen_;

    ins.mask = 1;
    ins.src = (unsigned char *)in;
    ins.srcend = (unsigned char *)in + insize;
    ins.ptr = ins.src;
    ins.buf = *ins.ptr++;

    magic[0] = (unsigned char)gz_readbits(&ins, 8);
    magic[1] = (unsigned char)gz_readbits(&ins, 8);
    if(magic[0] != 0x1f || magic[1] != 0x8b)
    {
        return(GZ_INVMAGIC);
    }

    cm = gz_readbits(&ins, 8);
    if(cm != 8)
    {
        return(GZ_INVCMETHOD);
    }

    flags = gz_readbits(&ins, 8);
    gz_readbits(&ins, 8);
    gz_readbits(&ins, 8);
    gz_readbits(&ins, 8);
    gz_readbits(&ins, 8);
    gz_readbits(&ins, 8);
    gz_readbits(&ins, 8);

    if(flags & FEXTRA)
    {
        xlen = gz_readbits(&ins, 2*8);
        for(i = 0;
            i < xlen;
            ++i)
        {
            gz_readbits(&ins, 8);
        }
    }

    if(flags & FNAME)
    {
        i = gz_readbits(&ins, 8);
        while(i)
        {
            i = gz_readbits(&ins, 8);
        }
    }

    if(flags & FCOMMENT)
    {
        i = gz_readbits(&ins, 8);
        while(i)
        {
            i = gz_readbits(&ins, 8);
        }
    }

    if(flags & FHCRC)
    {
        gz_readbits(&ins, 2*8);
    }

    islast = 0;
    while(!islast)
    {
        islast = gz_readbits(&ins, 1);
        btype = gz_readbits(&ins, 2);
        todec = 0;

        if(btype == 0)
        {
            /* Emit literals */
            b0len = gz_readbits(&ins, 8);
            b0nlen = gz_readbits(&ins, 8);
            if(b0len != ~b0nlen)
            {
                return(GZ_INVFILE);
            }

            while(b0len > 0)
            {
                EMIT(gz_readbits(&ins, 8));
                --b0len;
            }
        }
        else if(btype == 1)
        {
            /* Fixed Huffman tables */
            for(i = 0;
                i <= 143;
                ++i)
            {
                arrll[i] = 8;
            }

            for(i = 144;
                i <= 255;
                ++i)
            {
                arrll[i] = 9;
            }

            for(i = 256;
                i <= 279;
                ++i)
            {
                arrll[i] = 7;
            }

            for(i = 280;
                i <= 287;
                ++i)
            {
                arrll[i] = 8;
            }

            for(i = 0;
                i < DISTLEN;
                ++i)
            {
                arrdist[i] = 5;
            }

            if(!gz_buildht(arrll, LLLEN, htll, GZ_HTLL_MAX))
            {
                return(GZ_INVFILE);
            }

            if(!gz_buildht(arrdist, DISTLEN, htdist, GZ_HTDIST_MAX))
            {
                return(GZ_INVFILE);
            }

            todec = 1;
        }
        else if(btype == 2)
        {
            /* Dynamic Huffman tables */
            hlit = gz_readbits(&ins, 5);
            hdist = gz_readbits(&ins, 5);
            hclen = gz_readbits(&ins, 4);

            if(257 + hlit > LLLEN)
            {
                return(GZ_INVFILE);
            }

            /* Construct CL Lengths table */
            for(i = 0;
                i < hclen + 4;
                ++i)
            {
                arrclen[clenord[i]] = gz_readbits(&ins, 3);
            }

            if(!gz_buildht(arrclen, MAXCLEN, htclen, GZ_HTCLEN_MAX))
            {
                return(GZ_INVFILE);
            }

            if(!gz_gethufft(
                &ins, arrll, 257 + hlit,
                LLLEN, htclen,
                htll, GZ_HTLL_MAX))
            {
                return(GZ_INVFILE);
            }

            if(!gz_gethufft(
                &ins, arrdist, 1 + hdist,
                DISTLEN, htclen,
                htdist, GZ_HTDIST_MAX))
            {
                return(GZ_INVFILE);
            }

            todec = 1;
        }
        else
        {
            return(GZ_INVFILE);
        }

        if(todec)
        {
            sym = gz_huffdec(&ins, htll);
            while(sym != 256)
            {
                if(sym < 0 || sym > LLLEN)
                {
                    return(GZ_INVFILE);
                }

                if(sym >= 0 && sym <= 255)
                {
                    EMIT(sym);
                }
                else if(sym == 256)
                {
                    break;
                }
                else if(sym < LLLEN)
                {
                    len = gz_getlen(sym, &ins);
                    dist = gz_huffdec(&ins, htdist);
                    dist = gz_getdist(dist, &ins);

                    if(dist < 0 || len <= 0)
                    {
                        return(GZ_INVFILE);
                    }

                    backp = outp - dist;
                    if(outp < (unsigned char *)out)
                    {
                        return(GZ_INVFILE);
                    }

                    while(len > 0)
                    {
                        EMIT(*backp);
                        ++backp;
                        len -= 1;
                    }
                }
                else
                {
                    return(GZ_INVFILE);
                }

                sym = gz_huffdec(&ins, htll);
            }
        }
    }


    return(GZ_OK);

#undef EMIT
#undef DISTCLEN
#undef LLCLEN
#undef MAXCLEN
#undef FTEXT
#undef FHCRC
#undef FEXTRA
#undef FNAME
#undef FCOMMENT
}

unsigned int
gzdecsize(void *in, unsigned int insize)
{
    unsigned int decsize;
    unsigned char *inp;

    if(!in || insize < 18)
    {
        return(0);
    }

    decsize = 0;
    inp = (unsigned char *)in + insize - 4;
    decsize |= (unsigned int)*inp++;
    decsize |= ((unsigned int)*inp++) << 8;
    decsize |= ((unsigned int)*inp++) << 16;
    decsize |= ((unsigned int)*inp++) << 24;

    return(decsize);
}

#endif
#endif

#endif
