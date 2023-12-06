# concurrent-b-skip-list
Implementation of concurrent b skip list
## How to build
```
make BSkipList
./BSkipList 1 1 1 1 5
```



## test_data_thr instruction: test throughput
readers_0 (1000000 queries included): ratio of readers = 1 (only query)

writers0 (1000000 insertions included): ratio of readers = 0 (only insertion)

readers_1(500000 queries included) + writer1(500000 insertions included):ratio of readers = 0.5

readers_2(300000 queries included) + writer3(700000 insertions included):ratio of readers = 0.3

readers_3(700000 queries included) + writer2(300000 insertions included):ratio of readers = 0.7


## test_data_TR instruction: test time response
readers_0: 1000000 queries included

readers_1: 100000 queries included

readers_2: 10000 queries included

writers_0: 1000000 insertions included

writers_1: 100000 insertions included

writers_2: 10000 insertions included
