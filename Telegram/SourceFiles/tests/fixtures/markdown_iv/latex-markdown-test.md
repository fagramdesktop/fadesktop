# LaTeX in Markdown — Ultimate Test File

This document tests every common (and uncommon) way LaTeX appears in Markdown rendering.

---

## 1. Basic Inline Math

Single variable: $x$, $y$, $\alpha$, $\beta$, $\pi$.

In a sentence: The famous identity $e^{i\pi} + 1 = 0$ connects five fundamental constants.

Multiple inline: We have $a = 1$, $b = 2$, and $c = a + b = 3$.

Inline with operators: $f(x) = x^2 + 2x - 1$ evaluated at $x = 3$ gives $f(3) = 14$.

Greek letters: $\alpha$, $\beta$, $\gamma$, $\delta$, $\epsilon$, $\zeta$, $\eta$, $\theta$, $\iota$, $\kappa$, $\lambda$, $\mu$, $\nu$, $\xi$, $\pi$, $\rho$, $\sigma$, $\tau$, $\upsilon$, $\phi$, $\chi$, $\psi$, $\omega$.

Capital Greek: $\Gamma, \Delta, \Theta, \Lambda, \Xi, \Pi, \Sigma, \Upsilon, \Phi, \Psi$.

---

## 2. Display Math

Standard display:

$$
E = mc^2
$$

The quadratic formula:

$$
x = \frac{-b \pm \sqrt{b^2 - 4ac}}{2a}
$$

Euler's identity:

$$
e^{i\pi} + 1 = 0
$$

---

## 3. Fractions, Roots, and Powers

Inline fractions: $\frac{1}{2}$, $\frac{a+b}{c-d}$, $\frac{x^2}{y^3}$.

Nested fractions: $\frac{1}{1 + \frac{1}{1 + \frac{1}{x}}}$.

Display fraction:

$$
\frac{\partial f}{\partial x} = \lim_{h \to 0} \frac{f(x+h) - f(x)}{h}
$$

Roots: $\sqrt{2}$, $\sqrt[3]{27}$, $\sqrt[n]{x^n} = x$.

Display roots:

$$
\sqrt{1 + \sqrt{1 + \sqrt{1 + \sqrt{1 + \cdots}}}} = \varphi
$$

Powers and subscripts: $x^2$, $x_i$, $x_i^2$, $x^{i+j}$, $x_{i,j}^{2k}$.

---

## 4. Sums, Products, Integrals, Limits

Inline: $\sum_{i=1}^{n} i$, $\prod_{i=1}^{n} i$, $\int_0^1 x \, dx$, $\lim_{x \to 0} \frac{\sin x}{x}$.

Display sum:

$$
\sum_{i=1}^{n} i = \frac{n(n+1)}{2}
$$

Display product:

$$
\prod_{p \text{ prime}} \frac{1}{1 - p^{-s}} = \sum_{n=1}^{\infty} \frac{1}{n^s}
$$

Definite integral:

$$
\int_{-\infty}^{\infty} e^{-x^2} \, dx = \sqrt{\pi}
$$

Double integral:

$$
\iint_D f(x, y) \, dA
$$

Contour integral:

$$
\oint_C \mathbf{F} \cdot d\mathbf{r} = \iint_S (\nabla \times \mathbf{F}) \cdot d\mathbf{S}
$$

Limits:

$$
\lim_{n \to \infty} \left(1 + \frac{1}{n}\right)^n = e
$$

---

## 5. Matrices and Arrays

Plain matrix:

$$
\begin{matrix}
a & b \\
c & d
\end{matrix}
$$

Parenthesized:

$$
\begin{pmatrix}
1 & 2 & 3 \\
4 & 5 & 6 \\
7 & 8 & 9
\end{pmatrix}
$$

Bracketed:

$$
\begin{bmatrix}
1 & 0 & 0 \\
0 & 1 & 0 \\
0 & 0 & 1
\end{bmatrix}
$$

Determinant (vertical bars):

$$
\begin{vmatrix}
a & b \\
c & d
\end{vmatrix} = ad - bc
$$

Double bars (norm):

$$
\begin{Vmatrix}
v_1 \\
v_2
\end{Vmatrix}
$$

Curly braces:

$$
\begin{Bmatrix}
x \\
y
\end{Bmatrix}
$$

---

## 6. Cases and Piecewise Functions

$$
f(x) = \begin{cases}
x^2 & \text{if } x \geq 0 \\
-x^2 & \text{if } x < 0
\end{cases}
$$

$$
\text{sgn}(x) = \begin{cases}
1 & x > 0 \\
0 & x = 0 \\
-1 & x < 0
\end{cases}
$$

---

## 7. Aligned Equations

$$
\begin{aligned}
(a + b)^2 &= a^2 + 2ab + b^2 \\
(a - b)^2 &= a^2 - 2ab + b^2 \\
(a + b)(a - b) &= a^2 - b^2
\end{aligned}
$$

Multi-step derivation:

$$
\begin{aligned}
\nabla \times (\nabla \times \mathbf{A}) &= \nabla(\nabla \cdot \mathbf{A}) - \nabla^2 \mathbf{A} \\
&= \nabla(\nabla \cdot \mathbf{A}) - \Delta \mathbf{A}
\end{aligned}
$$

---

## 8. Operators and Symbols

Logic: $\land, \lor, \neg, \implies, \iff, \forall, \exists, \nexists$.

Set theory: $\in, \notin, \subset, \subseteq, \supset, \supseteq, \cup, \cap, \emptyset, \setminus$.

Comparison: $<, >, \leq, \geq, \neq, \approx, \equiv, \sim, \simeq, \cong, \propto$.

Arrows: $\to, \gets, \leftrightarrow, \Rightarrow, \Leftarrow, \Leftrightarrow, \mapsto, \hookrightarrow$.

Misc: $\infty, \partial, \nabla, \pm, \mp, \times, \div, \cdot, \circ, \star, \ast$.

Number sets: $\mathbb{N}, \mathbb{Z}, \mathbb{Q}, \mathbb{R}, \mathbb{C}, \mathbb{H}, \mathbb{F}_p$.

---

## 9. Functions and Operators

Trig: $\sin x, \cos x, \tan x, \csc x, \sec x, \cot x$.

Inverse trig: $\arcsin x, \arccos x, \arctan x$.

Hyperbolic: $\sinh x, \cosh x, \tanh x$.

Logs: $\log x, \ln x, \log_2 x, \log_{10} x$.

Other: $\exp x, \min, \max, \sup, \inf, \arg\min, \arg\max, \det, \dim, \ker, \deg, \gcd, \lim, \limsup, \liminf$.

---

## 10. Accents and Decorations

Bar: $\bar{x}, \overline{ABC}$.

Hat: $\hat{x}, \widehat{xyz}$.

Tilde: $\tilde{x}, \widetilde{xyz}$.

Vector: $\vec{v}, \overrightarrow{AB}$.

Dot: $\dot{x}, \ddot{x}, \dddot{x}$.

Bold/script: $\mathbf{v}, \mathbf{0}, \boldsymbol{\alpha}, \mathcal{L}, \mathfrak{g}, \mathscr{F}$.

---

## 11. Probability and Statistics

Expected value: $\mathbb{E}[X] = \int_{-\infty}^{\infty} x f(x) \, dx$.

Variance: $\text{Var}(X) = \mathbb{E}[(X - \mu)^2]$.

Normal distribution:

$$
f(x \mid \mu, \sigma^2) = \frac{1}{\sqrt{2\pi\sigma^2}} \exp\left(-\frac{(x-\mu)^2}{2\sigma^2}\right)
$$

Bayes' theorem:

$$
P(A \mid B) = \frac{P(B \mid A) \, P(A)}{P(B)}
$$

---

## 12. Edge Cases and Common Pitfalls

### Dollar signs as currency (should NOT render as math)

The shirt costs \$20 and the pants cost \$30. Together: \$50.

### Dollar signs near math

I have $5 in my pocket, not $5x + 3$ dollars.

### Underscores in inline math vs Markdown

Variables like $x_1, x_2, x_3$ should render correctly without triggering italic.

### Backslashes in text

Use `\frac{a}{b}` to write $\frac{a}{b}$.

### Math inside lists

1. First item with math: $a^2 + b^2 = c^2$
2. Second with display:
   $$
   \int_0^\infty e^{-x} \, dx = 1
   $$
3. Third with fraction: $\frac{p}{q}$

### Math inside blockquotes

> The Cauchy-Schwarz inequality:
> $$
> \left(\sum_{i=1}^n a_i b_i\right)^2 \leq \left(\sum_{i=1}^n a_i^2\right)\left(\sum_{i=1}^n b_i^2\right)
> $$

### Math in tables

| Function | Derivative | Integral |
|----------|------------|----------|
| $x^n$ | $nx^{n-1}$ | $\frac{x^{n+1}}{n+1}$ |
| $e^x$ | $e^x$ | $e^x$ |
| $\sin x$ | $\cos x$ | $-\cos x$ |
| $\ln x$ | $\frac{1}{x}$ | $x \ln x - x$ |

### Math in headers

#### The equation $ax^2 + bx + c = 0$

### Bold and italic with math

**Bold text with $E = mc^2$ inline.**

*Italic with $\pi \approx 3.14$ inline.*

### Code blocks should NOT render math

```
This is a code block: $x^2 + y^2 = z^2$ — should be literal.
```

Inline code: `$x^2$` — also literal.

### Escaped dollar signs

To write a literal dollar sign: \$100 or \$x^2\$.

---

## 13. Long and Complex Expressions

Maxwell's equations:

$$
\begin{aligned}
\nabla \cdot \mathbf{E} &= \frac{\rho}{\varepsilon_0} \\
\nabla \cdot \mathbf{B} &= 0 \\
\nabla \times \mathbf{E} &= -\frac{\partial \mathbf{B}}{\partial t} \\
\nabla \times \mathbf{B} &= \mu_0 \mathbf{J} + \mu_0 \varepsilon_0 \frac{\partial \mathbf{E}}{\partial t}
\end{aligned}
$$

Schrödinger equation:

$$
i\hbar \frac{\partial}{\partial t} \Psi(\mathbf{r}, t) = \left[-\frac{\hbar^2}{2m} \nabla^2 + V(\mathbf{r}, t)\right] \Psi(\mathbf{r}, t)
$$

Einstein field equations:

$$
R_{\mu\nu} - \frac{1}{2} R g_{\mu\nu} + \Lambda g_{\mu\nu} = \frac{8\pi G}{c^4} T_{\mu\nu}
$$

Fourier transform:

$$
\hat{f}(\xi) = \int_{-\infty}^{\infty} f(x) \, e^{-2\pi i x \xi} \, dx
$$

---

## 14. Spacing Tests

No space: $a+b$, $a-b$, $a=b$.

Thin space: $a\,b$.

Medium space: $a\:b$.

Thick space: $a\;b$.

Quad: $a\quad b$.

Double quad: $a\qquad b$.

Negative space: $a\!b$.

---

## 15. Stretchy Delimiters

$$
\left( \frac{a}{b} \right), \quad \left[ \frac{a}{b} \right], \quad \left\{ \frac{a}{b} \right\}, \quad \left| \frac{a}{b} \right|, \quad \left\| \frac{a}{b} \right\|
$$

$$
\left( \sum_{i=1}^{n} \frac{x_i^2}{\sqrt{1 + x_i^4}} \right)^{1/2}
$$
