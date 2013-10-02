var ed = require('./');

function findFirstDAC(callback) {
  ed.start();
  // Wait a second for the DACs to ping the network.
  setTimeout(function() {
    if (ed.getCount() < 1) {
      throw 'Error: Couldn’t find any Ether Dreams on the net';
    }
    callback(new ed.Etherdream(0));
  }, 1200);
}

var dac;
var pointsPerSecond = 30000;
var pointsPerFrame = 400;
var points = new Uint16Array(pointsPerFrame * 8);
var startTime;

function step() {
  var time = (Date.now() - startTime) / 1000;

  // Superformula params
  var params = {
    m: 6,
    n1: 3 + Math.sin(time) * 2,
    n2: Math.cos(time * 0.33) * 6,
    n3: 2 + Math.sin(time * 0.55),
    a: 1,
    b: 1
  };

  // Compute points
  var pts = genSuperformulaPoints(params, pointsPerFrame);
  for (var i = 0, j = 0; i < pointsPerFrame; ++i, j += 8) {
    points[j    ] = Math.floor(pts[i].x * 32767);
    points[j + 1] = Math.floor(pts[i].y * 32767);
    points[j + 2] = 65535;
    points[j + 3] = 65535 * 0.33;
    points[j + 4] = 65535 * 0.11;
  }

  dac.write(points, pointsPerSecond, 1);

  // Queue up the next frame.
  dac.whenReady(step);
}

function start(foundDac) {
  dac = foundDac;
  dac.connect();

  // Queue the first frame.
  dac.whenReady(step);

  startTime = Date.now();
}

findFirstDAC(start);


// From D3.js superformula plugin
// https://github.com/d3/d3-plugins/blob/master/superformula/superformula.js#L46
function genSuperformulaPoints(p, n) {
  var r = 0, i, t, dt = 2 * Math.PI / n;
  var lens = [], points = [];

  for (i = 0; i < n; ++i) {
    t = p.m * (i * dt - Math.PI) / 4;
    t = Math.pow(Math.abs(Math.pow(Math.abs(Math.cos(t) / p.a), p.n2) +
        Math.pow(Math.abs(Math.sin(t) / p.b), p.n3)), -1 / p.n1);
    if (t > r) r = t;
    lens.push(t);
  }

  r = Math.SQRT1_2 / r;
  for (i = 0; i < n; ++i) {
    points.push({
      x: (t = lens[i] * r) * Math.cos(i * dt),
      y: t * Math.sin(i * dt)
    });
  }

  return points;
}
