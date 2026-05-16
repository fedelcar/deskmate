const fetch = require('node-fetch');

const ICON_MAP = {
  '01': 'CLEAR', '02': 'FEW_CLOUDS', '03': 'CLOUDS', '04': 'CLOUDS',
  '09': 'RAIN', '10': 'RAIN', '11': 'STORM', '13': 'SNOW', '50': 'MIST',
};

function mapIcon(icon) {
  return ICON_MAP[icon.slice(0, 2)] || 'CLEAR';
}

async function getWeather() {
  const { OWM_API_KEY, LATITUDE, LONGITUDE, UNITS = 'imperial' } = process.env;
  if (!OWM_API_KEY) throw new Error('OWM_API_KEY not set');

  const unit = UNITS === 'metric' ? 'metric' : 'imperial';
  const deg = unit === 'imperial' ? 'F' : 'C';

  const [currentRes, forecastRes] = await Promise.all([
    fetch(`https://api.openweathermap.org/data/2.5/weather?lat=${LATITUDE}&lon=${LONGITUDE}&appid=${OWM_API_KEY}&units=${unit}`),
    fetch(`https://api.openweathermap.org/data/2.5/forecast?lat=${LATITUDE}&lon=${LONGITUDE}&appid=${OWM_API_KEY}&units=${unit}&cnt=9`),
  ]);

  if (!currentRes.ok) throw new Error(`OWM current: ${currentRes.status}`);
  if (!forecastRes.ok) throw new Error(`OWM forecast: ${forecastRes.status}`);

  const current = await currentRes.json();
  const forecast = await forecastRes.json();

  const hourly = forecast.list.map(h => ({
    time: h.dt * 1000,
    temp: Math.round(h.main.temp),
    icon: mapIcon(h.weather[0].icon),
    pop: Math.round((h.pop || 0) * 100),
  }));

  // Daily high/low from forecast list (today's entries)
  const todayTemps = forecast.list.filter(h => {
    const d = new Date(h.dt * 1000);
    const today = new Date();
    return d.getDate() === today.getDate();
  }).map(h => h.main.temp);

  return {
    temp: Math.round(current.main.temp),
    feels_like: Math.round(current.main.feels_like),
    description: current.weather[0].description,
    icon: mapIcon(current.weather[0].icon),
    humidity: current.main.humidity,
    wind_speed: Math.round(current.wind.speed),
    high: todayTemps.length ? Math.round(Math.max(...todayTemps)) : Math.round(current.main.temp_max),
    low: todayTemps.length ? Math.round(Math.min(...todayTemps)) : Math.round(current.main.temp_min),
    deg,
    hourly,
  };
}

module.exports = { getWeather };
