## Mon Jan  4 02:14:01 CST 2021

It is decided that arrays are phoney arrays. Also, we might want some sort of
memory locking in case we end up using regions for network write buffers.

Need to spend a day reading about pthreads and memory.

## Mon Jan  4 02:09:50 CST 2021

Either you allow circular references, which I don't believe you will do, or you
check for circular references on an object move, or you insist on copying.

Of the three, the check for circular references is best. We are going to make
the stack a common component.

A maximum object depth should be a property of the heap so we don't exhaust the
stack whether it is internal or system.

## Mon Jan  4 02:03:45 CST 2021

More of a memory management and interchange format. Probably not suitable for
embedded unless that system is a Linux system. Can imagine this being useful for
interchange or writing H20 or libuv servers. Came across [Tiny Garbage
Collector](https://github.com/orangeduck/tgc) somehow, and Jasson used
[Boehm-Demers-Weiser](https://www.hboehm.info/gc/) in an example of how to
replace the system `malloc` and `free`. [Writing a Simple Garbage Collector in
C](https://maplant.com/gc.html) and [Baby's First Garbage
Collector](https://journal.stuffwithstuff.com/2013/12/08/babys-first-garbage-collector/)
are two articles on how to write a garbage collector.

What we're building here is basically a reference ownership system.

## Mon Jan  4 00:50:04 CST 2021

Proceding more slowly with CMake. Currently working on adding unit testing.

Parsing can be supported using [JSMN](https://zserge.com/jsmn/). There is no
string parsing, but there is no need for this library to do string manipulation,
so we should convert code points, maybe validate utf8.

JSMN will skip over the body of a string correctly. Someone could submit a
decoder that convert to UTF-8 including converting the `\u` escapes.

I've chosen Criterion as a unit test framework. It has CMake examples so it
expedites the process of getting unit tests working. Looked at
[Check](https://libcheck.github.io/check/) which I somehow remember having a
prettier web page. Found it from [Comparision of Unit Test
Frameworks](http://www.throwtheswitch.org/comparison-of-unit-test-frameworks).

Projects worth considering because their websites are so pretty are
[μnit](https://nemequ.github.io/munit/) and [cmocka](https://cmocka.org/).
