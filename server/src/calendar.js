const ical = require('node-ical');

function expandRecurring(event, start, end) {
  // node-ical handles RRULE expansion — we just need to extract the right occurrences
  if (!event.rrule) return [];
  try {
    const dates = event.rrule.between(start, end);
    return dates.map(d => ({ ...event, start: d, end: new Date(d.getTime() + (event.end - event.start)) }));
  } catch {
    return [];
  }
}

async function getEvents() {
  const { ICAL_URL } = process.env;
  if (!ICAL_URL) throw new Error('ICAL_URL not set');

  const events = await ical.async.fromURL(ICAL_URL);

  const now = new Date();
  const windowEnd = new Date(now.getTime() + 7 * 24 * 60 * 60 * 1000);

  const results = [];

  for (const event of Object.values(events)) {
    if (event.type !== 'VEVENT') continue;

    let occurrences = [];

    if (event.rrule) {
      occurrences = expandRecurring(event, now, windowEnd);
    } else if (event.start >= now && event.start <= windowEnd) {
      occurrences = [event];
    }

    for (const occ of occurrences) {
      if (occ.start < now) continue;
      results.push({
        title: occ.summary || '(no title)',
        start: occ.start.toISOString(),
        end: occ.end ? occ.end.toISOString() : null,
        minutes_until: Math.round((occ.start - now) / 60000),
        all_day: !occ.start.getHours() && !occ.start.getMinutes(),
      });
    }
  }

  results.sort((a, b) => new Date(a.start) - new Date(b.start));
  return results.slice(0, 5);
}

module.exports = { getEvents };
