# Shard

Shard is a programming language designed for declarative builds and configuration of the Amethyst Operating System.

Shard is a...

- declarative
- pure
- functional
- lazy
- dynamically typed

language majorly inspired by Nix and NixOS and is implemented as a strict superset of `nix`. It therefore shares most core principles with the well-established nix package manager and linux distribution.

## TODO:

- String interpolation
- String interpolation in paths
- String interpolation in attribute names
- `__functor` attribute for sets

## Building

There are no dependencies other that `libc` to the pure implementation of shard.

To build the interpreter, just run `make` in this directory:

```sh
$ make
```

If successful, the interpreter binary will be placed in `build/shard`. It can be invoked using `./build/shard <file.shard>`

## Examples

There are a few example programs in the `examples/` directory

## Editor Support

TODO

## Language Overview

|  Shard code           | Description    |
|-----------------------|----------------|
| *Basic values*        |                |
| `true` , `false`      | Boolean values |
| `null`                | Null value     |
| 69, 420               | Integer values |
| 3.1415                | Float values   |
| `"Hello, World"`      | String literal |
| `"foo ${bar}"`        | String interpolation |
| `/path`, `~/path`, `./path` | Absolute and relative path literals |
| `<path>`              | Shard search path |
| *Compound values*     |                   |
| `{ x = 69; y = 420; }` | Attribute Sets   |
| `{ x.y = 42; }`       | Nested attribute sets |
| `rec { x = 34; y = x + 35; }` | Recursive attribute sets |
| `[ "foo" "bar" "baz" ]` | Linked lists
| *Prefix Operators*     |               |
| `-number`              | Numeric negation |
| `!boolean`             | Logical not      |
| *Infix Operators*      |              |
| `bool \|\| bool`       | Logical or |
| `bool && bool`         | Logical and |
| `bool -> bool`         | Logical implication |
| `expr == expr`         | Equality |
| `expr != expr`         | Inequality |
| `expr > expr`, `expr < expr`, `expr >= expr`, `expr <= expr` | Relational comparison (greater than, less than, ...)
| `set // set`          | Attribute  set combination |
| `set ? attr`          | Attribute check |
| `set . attr [or value]` | Attribute selection |
| `list ++ list`        | List append   |
| `expr + expr`         | Numerical or textual (string) addition |
| `expr - expr`         | Subtraction   |
| `expr * expr`         | Multiplication |
| `expr / expr`         | Division |
| `func expr`           | Function call |
| *Control Structures*  |               |
| `if bool then "yes" else "no"` | Conditional/Ternary expression |
| `case value of ...`   | Case (`switch`) expression |
| `assert value; "pass"` | Assertion check |
| `let x = 34; y = 35; in x + y` | Let bindings |
| `with set; value`     | With expression -> Bind all attributes in a set to the current scope |
| *Functions*           |           |
| `x: x + 1`            | Function definition |
| `x: y: x + y`         | Curried function definition |
| `{ x, y }: x + y`     | Function definiton with set destructuring |
| `{ x ? "foo" }: x`    | Default attribute value |
| `{ x, ... }: x + 1`   | Ignored additional attributes |
| `{ x, ... } @ args: x + args.y` | Whole set binding to `args` |
| *Builtins*            |           |
| TODO                  |           |

## License

Shard is a part of the [Amethyst Operating System](https://github.com/spydr06/amethyst) and is therefore licensed under the [MIT license](../LICENSE).

```
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```
