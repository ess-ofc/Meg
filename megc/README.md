# The Meg Programming Language Compiler (`megc`)
   This repository contains the code of the Meg
compiler. The compiler is experimental, but the
idea is not to do bootstrapping. If you want to
contribute, contact-me, we need a standard library
and runtime. Also, the compiler is not done, feel
free to send any contribution.

## Some of its syntax
   The syntax will be like as follows:
```Meg
\ Declares a function
  named 'sum'. \ 
sum(x: i32, y: i32) := x + y

\ Main function \ 
main(): i32 = {
   x := sum(20, 30)
   => x \ Returns x, that is 50 \ 
}
```
NOTE: Nothing works, yet...

*In memory of Meg, my dog, 2020-2025.*
