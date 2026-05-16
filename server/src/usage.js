const fs = require('fs');
const path = require('path');
const readline = require('readline');
const os = require('os');

async function getUsage() {
  const historyPath = path.join(os.homedir(), '.claude', 'history.jsonl');

  const now = new Date();
  const monthStart = new Date(now.getFullYear(), now.getMonth(), 1);
  const nextMonthStart = new Date(now.getFullYear(), now.getMonth() + 1, 1);
  const daysUntilReset = Math.ceil((nextMonthStart - now) / (1000 * 60 * 60 * 24));

  const sessions = new Set();
  const dailySessions = new Set();
  const todayStart = new Date(now.getFullYear(), now.getMonth(), now.getDate());
  let messagesThisMonth = 0;
  let messagesToday = 0;

  try {
    const stream = fs.createReadStream(historyPath);
    const rl = readline.createInterface({ input: stream, crlfDelay: Infinity });

    for await (const line of rl) {
      if (!line.trim()) continue;
      let entry;
      try { entry = JSON.parse(line); } catch { continue; }

      const ts = new Date(entry.timestamp);
      if (isNaN(ts)) continue;

      if (ts >= monthStart && ts < nextMonthStart) {
        if (entry.sessionId) sessions.add(entry.sessionId);
        messagesThisMonth++;
        if (ts >= todayStart) {
          if (entry.sessionId) dailySessions.add(entry.sessionId);
          messagesToday++;
        }
      }
    }
  } catch (err) {
    if (err.code !== 'ENOENT') console.error('history.jsonl read error:', err.message);
  }

  return {
    sessions_this_month: sessions.size,
    messages_this_month: messagesThisMonth,
    sessions_today: dailySessions.size,
    messages_today: messagesToday,
    reset_date: nextMonthStart.toISOString().split('T')[0],
    days_until_reset: daysUntilReset,
    month: now.toLocaleString('default', { month: 'long' }),
  };
}

module.exports = { getUsage };
