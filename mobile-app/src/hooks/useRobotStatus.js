import { useState, useEffect, useRef } from 'react';
import { fetchHealth } from '../cerebroApi';

export default function useRobotStatus(pollMs) {
  var interval = pollMs || 15000;
  var ref = useRef(null);
  var _s = useState({
    cerebroOnline: false,
    sleeping: false,
  });
  var status = _s[0];
  var setStatus = _s[1];

  var poll = function() {
    fetchHealth()
      .then(function(h) {
        setStatus({ cerebroOnline: true, sleeping: h.sleeping });
      })
      .catch(function() {
        setStatus(function(prev) { return { ...prev, cerebroOnline: false }; });
      });
  };

  useEffect(function() {
    poll();
    ref.current = setInterval(poll, interval);
    return function() { clearInterval(ref.current); };
  }, []);

  return status;
}
