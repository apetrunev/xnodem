var gtm = require('nodem');

var db = new gtm.Gtm();
db.open();

node = { global: 'dlw', subscripts: ['testing', 1], data: 'какой-то тест '};
ret = db.set(node);
node = { global: 'dlw', subscripts: ['testing', 1]};
ret = db.get(node);
console.log("db.get(): ret "  + JSON.stringify(ret));
node = { global: 'dlw', subscripts: ['testing', 1]};
ret = db.order(node);
console.log("db.order() " + JSON.stringify(ret));
db.close();

