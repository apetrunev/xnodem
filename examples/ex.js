var gtm = require('nodem');
//var gtm = require('../lib/nodem');
var db = new gtm.Gtm();
var iks = new gtm.IKS();
var crypto = require('crypto');

var ret = db.open();
console.log(ret);

var sessionId = crypto.randomBytes(20).toString('hex');
var ret = iks.login({ "key": sessionId, "uid" : "470", "pass1": "2810", "pass2": "" });

console.log(ret);

ret = db.close();
console.log(ret);
