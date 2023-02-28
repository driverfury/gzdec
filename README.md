# GZIP in-memory decompressor library

## Description
A very simple GZIP in-memory decompressor library with a 2-functions API.

```c
unsigned int gzdecsize(void *in, unsigned int insize);
int gzdec(void *in, unsigned int insize, void *out, unsigned int outsize);
```

## Usage
1. Define ```GZDEC_IMPLEMENTATION``` and then include the library:
```c
#define GZDEC_IMPLEMENTATION
#include "gzdec.h"
```

2. Use it:
```c
void *in, *out;
unsigned int insize, outsize;
int result;

in = read_entire_file("myfile.bin.gz", &insize);
outsize = gzdecsize(in, insize);
out = malloc(outsize);
result = gzdec(in, insize, out, outsize);
if(result == GZ_OK)
{
    write_entire_file("myfile.bin", out, outsize);
}
else
{
    printf("Cannot decompress file");
}
```
