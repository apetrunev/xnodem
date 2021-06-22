try {
    module.exports = require('../build/Release/mumps');
} catch (error) {
    try {
        module.exports = require('./mumps');
    } catch (err) {
        console.error(err.stack + '\n');

        if (error.code === 'MODULE_NOT_FOUND') {
            console.info("Try rebuilding Xnodem with 'npm run install'.");
        }
    }
}
