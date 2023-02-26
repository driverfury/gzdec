# GZIP in-memory decompressor

## Description
A very simple GZIP in-memory decompressor library with a one-function API.

## Usage
```c
void *in, *out;
unsigned int insize, outsize;
int result;

void *in = read_entire_file("myfile.bin.gz", &insize);
result = gzdec(in, insize, &out, &outsize);
if(result == GZ_OK)
{
  write_entire_file("myfile.bin", out, outsize);
}
else
{
  printf("Cannot decompress file");
}
```
