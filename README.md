# GZIP in-memory decompressor

## Description
A very simple GZIP in-memory decompressor library with a 2-functions API.

```c
unsigned int gzdecsize(void *in, unsigned int insize);
int gzdec(void *in, unsigned int insize, void *out, unsigned int outsize);
```

## Usage
```c
void *in, *out;
unsigned int insize, outsize;
int result;

void *in = read_entire_file("myfile.bin.gz", &insize);
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
