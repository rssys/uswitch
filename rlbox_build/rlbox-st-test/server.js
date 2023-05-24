'use strict';
const express = require('express')
const app = express()

app.use(express.static('static'))

function mkImages(nr, imgName) {
  let str = "";
  for(let i = 0; i < nr; i++) {
    str += `<img width=60 src="http://host${i}.total${nr}.scaling.localhost:1337/${imgName}"/>\n`;
  }
  return str;
}

app.get('/jpeg-test', function (req, res) {
  const nrImages =  req.query.nr || 10;
  res.send(`<html><head></head><body>
    ${mkImages(nrImages, "img_small.jpeg")}
    </body></html>`);
})

app.get('/jpeg-test-large', function (req, res) {
  const nrImages =  req.query.nr || 10;
  res.send(`<html><head></head><body>
    ${mkImages(nrImages, "img_large.jpeg")}
    </body></html>`);
})

app.listen(1337)
console.log("Started!")

// http://localhost:1337/ - zlib test
// http://localhost:1337/jpeg-test?nr=15 - sandbox scaling test
// http://localhost:1337/jpeg-test-large?nr=15 - sandbox scaling test
