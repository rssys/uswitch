const mylib = require('./build/Release/mylib_node');

// call the library hello function
mylib.hello();

// call the add function and check the result:
console.log(`Adding... 3+4 = ${mylib.add(3,4)}`);

// call the library echo function
mylib.echo("hi hi!");
