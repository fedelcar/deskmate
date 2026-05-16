require('dotenv').config({ path: require('path').join(__dirname, '../.env') });

const express = require('express');
const { Bonjour } = require('bonjour-service');
const { getWeather } = require('./weather');
const { getEvents } = require('./calendar');
const { getUsage } = require('./usage');

const app = express();
const PORT = parseInt(process.env.PORT || '3001');

// Simple in-memory cache so the ESP32 polling at 60s doesn't hammer APIs
const cache = {};
const TTL = { weather: 5 * 60 * 1000, calendar: 2 * 60 * 1000, usage: 10 * 60 * 1000 };

async function cached(key, fn) {
  const now = Date.now();
  if (cache[key] && now - cache[key].ts < TTL[key]) return cache[key].data;
  const data = await fn();
  cache[key] = { data, ts: now };
  return data;
}

app.get('/api/weather', async (req, res) => {
  try { res.json(await cached('weather', getWeather)); }
  catch (e) { res.status(500).json({ error: e.message }); }
});

app.get('/api/calendar', async (req, res) => {
  try { res.json(await cached('calendar', getEvents)); }
  catch (e) { res.status(500).json({ error: e.message }); }
});

app.get('/api/usage', async (req, res) => {
  try { res.json(await cached('usage', getUsage)); }
  catch (e) { res.status(500).json({ error: e.message }); }
});

// Single endpoint the ESP32 calls — returns everything at once
app.get('/api/all', async (req, res) => {
  try {
    const [weather, calendar, usage] = await Promise.allSettled([
      cached('weather', getWeather),
      cached('calendar', getEvents),
      cached('usage', getUsage),
    ]);
    res.json({
      weather: weather.status === 'fulfilled' ? weather.value : { error: weather.reason?.message },
      calendar: calendar.status === 'fulfilled' ? calendar.value : [],
      usage: usage.status === 'fulfilled' ? usage.value : { error: usage.reason?.message },
      ts: Date.now(),
    });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.get('/health', (req, res) => res.json({ ok: true }));

app.listen(PORT, '0.0.0.0', () => {
  console.log(`\nDeskmate server running on port ${PORT}`);
  console.log(`Local:   http://localhost:${PORT}/api/all`);

  // Advertise via mDNS so the ESP32 can find us as "deskmate.local"
  const bonjour = new Bonjour();
  bonjour.publish({ name: 'deskmate', type: 'http', port: PORT });
  console.log(`mDNS:    http://deskmate.local:${PORT}/api/all\n`);
});
