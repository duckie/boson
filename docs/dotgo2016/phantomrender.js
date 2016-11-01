var page = require('webpage').create();
page.open('http://127.0.0.1:8080/', function() {
    page.paperSize = { format:"A2", orientation:"landscape"}; 
    var i = 1;
    page.render('page' + i + '.pdf');
    ++i;
    for (; i < 25; ++i) {
      page.sendEvent('keypress', page.event.key.Space);
      page.render('page' + i + '.pdf');
    }
    phantom.exit();
});
