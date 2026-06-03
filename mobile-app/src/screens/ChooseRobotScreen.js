import React, { useState } from 'react';
import { View, Text, StyleSheet, Pressable } from 'react-native';
import AsyncStorage from '@react-native-async-storage/async-storage';

var ROBOTS = [
  { id: 'robot2', name: 'Rumi', role: 'Kid Companion', desc: 'Gentle, playful and always excited.', color: '#ff6eb4', bg: '#ffe0f0', textColor: '#c2185b' },
  { id: 'robot1', name: 'Kira', role: 'Main Host', desc: 'Warm, enthusiastic gaming nerd.', color: '#8b5cf6', bg: '#ede9fe', textColor: '#5b21b6' },
];

export default function ChooseRobotScreen(props) {
  var _s = useState('robot2');
  var selected = _s[0];
  var setSelected = _s[1];

  function save() {
    AsyncStorage.setItem('my_robot', selected);
    props.onSelect(selected);
  }

  var robot = ROBOTS.find(function(r) { return r.id === selected; });

  return (
    <View style={styles.container}>
      <Text style={styles.title}>{"Choose Your Robot"}</Text>
      <Text style={styles.sub}>{"Who's your companion?"}</Text>
      <View style={styles.cards}>
        {ROBOTS.map(function(r) {
          var isSel = selected === r.id;
          return (
            <Pressable key={r.id}
              style={[styles.card, isSel && { borderColor: r.color, backgroundColor: r.bg + '44' }]}
              onPress={function() { setSelected(r.id); }}>
              <View style={[styles.faceBox, { borderColor: r.color, backgroundColor: r.bg }]}>
                <Text style={styles.faceEmoji}>{r.id === 'robot2' ? '\u{1F9E1}' : '\u{1F49C}'}</Text>
              </View>
              <Text style={[styles.robotName, { color: r.color }]}>{r.name}</Text>
              <Text style={styles.robotRole}>{r.role}</Text>
              <Text style={styles.robotDesc}>{r.desc}</Text>
              {isSel && <View style={[styles.badge, { backgroundColor: r.color }]}><Text style={styles.badgeText}>{"Selected"}</Text></View>}
            </Pressable>
          );
        })}
      </View>
      <Pressable style={[styles.goBtn, { backgroundColor: robot.color }]} onPress={save}>
        <Text style={styles.goText}>{"Let's go with " + robot.name + "!"}</Text>
      </Pressable>
    </View>
  );
}

var styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0f0f13', alignItems: 'center', padding: 24, gap: 16 },
  title: { fontSize: 28, fontWeight: '900', color: 'white', textAlign: 'center', marginTop: 60 },
  sub: { fontSize: 14, color: '#555', textAlign: 'center' },
  cards: { flexDirection: 'row', gap: 14, width: '100%' },
  card: { flex: 1, backgroundColor: '#1a1a24', borderRadius: 20, padding: 16, alignItems: 'center', gap: 8, borderWidth: 2.5, borderColor: '#252535' },
  faceBox: { width: 90, height: 90, borderRadius: 20, borderWidth: 2.5, alignItems: 'center', justifyContent: 'center' },
  faceEmoji: { fontSize: 48 },
  robotName: { fontSize: 20, fontWeight: '900' },
  robotRole: { fontSize: 11, color: '#555', fontWeight: '700', textTransform: 'uppercase', letterSpacing: 1 },
  robotDesc: { fontSize: 12, color: '#666', textAlign: 'center', lineHeight: 18 },
  badge: { paddingHorizontal: 12, paddingVertical: 4, borderRadius: 20, marginTop: 4 },
  badgeText: { color: 'white', fontWeight: '800', fontSize: 12 },
  goBtn: { width: '100%', padding: 18, borderRadius: 16, alignItems: 'center', marginTop: 8 },
  goText: { color: 'white', fontWeight: '900', fontSize: 17 },
});
