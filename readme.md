# Cache Simulator
This project demonstrates the impact of cache memories on the performance of C programs by simulating the behavior of a cache memory.

## How to Run the Test
To run the cache simulator, 
```
make
```
then use the following command:
```
./csim -S <S> -K <K> -B <B> -p <P> -t <tracefile>
```
Command-Line Arguments
- -S \<S>: Number of sets
- -K \<K>: Number of lines per set (associativity)
- -B \<B>: Number of bytes per line
- -p \<P>: Eviction policy (LRU or FIFO)
- -t \<tracefile>: Name of the valgrind trace to replay

## Example Usage
For cache simulation with 16 sets, 1 line per set, 16 bytes per line, LRU eviction policy, and `sampleTraceFile.trace` as the trace file, use the following command:
```
$ ./csim -S 16 -K 1 -B 16 -p LRU -t sampleTraceFile.trace
```

## Making Custom Traces
To make custom traces, make a file(.trace file) with the following format:
```
[operation] [address,size]
[operation] [address,size]
[operation] [address,size]
```
Where:
- [operation] is either `I`, `L`, `S`, or `M` (Instruction load, Data load, Data store, Data modify)
- [address] is the memory address in hexadecimal
- [size] is the size of the data in bytes

## Example Trace
```
I 0400,4
 L 1100,8
 S 1100,8
 M 1100,8
 L 1200,8
 S 1200,8
 M 1200,8
```
