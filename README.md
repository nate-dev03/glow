Glow Programming Language
========================

*Glow* is a lightweight, dynamically typed programming language written in C. The language is largely inspired by Python, both in terms of syntax and implementation (CPython, to be exact).

Examples
--------

#### "hello world"

<pre>
<i># simple "hello world" program</i>
<b>echo</b> 'hello world'
</pre>

#### Arithmetic

<pre>
<b>echo</b> ((50 - 5*6)/4)**2  <i># prints 25</i>
</pre>

#### `if` statements

<pre>
b = 1
<b>if</b> b {
    <b>echo</b> "b is non-zero!"  <i># strings can be delimited by either " or '</i>
} <b>else</b> {
    <b>echo</b> "b is zero!"
}
</pre>

#### `while` statements

<pre>
<i># print integers from 1 to 9:</i>
a = 1
<b>while</b> a < 10 {
    <b>echo</b> a
    a += 1
}
</pre>

#### `for` statements

<pre>
<i># equivalent of while-loop above:</i>
<b>for</b> a <b>in</b> 1..10:  <i># ':' can be used for single-statement blocks</i>
    <b>echo</b> a
</pre>

#### Functions

<pre>
<i># factorial function</i>
<b>fun</b> fact(n) {
    <b>if</b> n < 2 { <b>return</b> 1 } <b>else</b> { <b>return</b> n * fact(n - 1) }
}

<i># hypotenuse function</i>
<b>fun</b> hypot(a, b) {
    <i># functions can be nested:</i>
    <b>fun</b> square(x) {
        <b>return</b> x**2
    }

    <i># functions can be anonymous:</i>
    sqrt = (: $1 ** 0.5)

    <b>return</b> sqrt(square(a) + square(b))
}
</pre>
