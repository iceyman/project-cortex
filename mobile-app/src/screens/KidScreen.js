import React, { useState, useRef, useEffect } from 'react';
import { View, Text, StyleSheet, Pressable, Animated, ScrollView, ActivityIndicator } from 'react-native';
import { Audio } from 'expo-av';
import * as Haptics from 'expo-haptics';
import * as KeepAwake from 'expo-keep-awake';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { sendAppVoice } from '../cerebroApi';
import useRobotStatus from '../hooks/useRobotStatus';

var ROBOTS = {
  robot2: { id: 'robot2', name: 'Rumi', color: '#ff6eb4', bg: '#ffe0f0', textColor: '#c2185b', mode: 'kid' },
  robot1: { id: 'robot1', name: 'Kira', color: '#8b5cf6', bg: '#ede9fe', textColor: '#5b21b6', mode: 'adult' },
};

function MicBars(props) {
  var active = props.active;
  var color = props.color;
  var bars = [
    useRef(new Animated.Value(0.3)).current,
    useRef(new Animated.Value(0.3)).current,
    useRef(new Animated.Value(0.3)).current,
    useRef(new Animated.Value(0.3)).current,
    useRef(new Animated.Value(0.3)).current,
  ];
  var loops = useRef([]);

  useEffect(function() {
    if (active) {
      loops.current = bars.map(function(b, i) {
        return Animated.loop(Animated.sequence([
          Animated.timing(b, { toValue: 0.3 + Math.random() * 0.7, duration: 150 + i * 50, useNativeDriver: true }),
          Animated.timing(b, { toValue: 0.2 + Math.random() * 0.3, duration: 150 + i * 50, useNativeDriver: true }),
        ]));
      });
      loops.current.forEach(function(l) { l.start(); });
    } else {
      loops.current.forEach(function(l) { l.stop(); });
      bars.forEach(function(b) { Animated.timing(b, { toValue: 0.3, duration: 200, useNativeDriver: true }).start(); });
    }
  }, [active]);

  return (
    <View style={styles.bars}>
      {bars.map(function(b, i) {
        return <Animated.View key={i} style={[styles.bar, { backgroundColor: color, transform: [{ scaleY: b }] }]} />;
      })}
    </View>
  );
}

export default function KidScreen(props) {
  var myRobot = props.myRobot || 'robot2';
  var robot = ROBOTS[myRobot] || ROBOTS.robot2;

  var _status = useState('ready');
  var status = _status[0];
  var setStatus = _status[1];

  var _msgs = useState([{ type: 'system', text: 'Hold the button to talk!' }]);
  var messages = _msgs[0];
  var setMessages = _msgs[1];

  var _kid = useState('buddy');
  var kidName = _kid[0];
  var setKidName = _kid[1];

  var recordingRef = useRef(null);
  var soundRef = useRef(null);
  var pulseAnim = useRef(new Animated.Value(1)).current;
  var pulseLoop = useRef(null);
  var robotStatus = useRobotStatus(15000);
  var scrollRef = useRef(null);

  useEffect(function() {
    Audio.requestPermissionsAsync();
    AsyncStorage.getItem('kid_name').then(function(n) { if (n) setKidName(n); });
    KeepAwake.activateKeepAwakeAsync();
    return function() { KeepAwake.deactivateKeepAwake(); };
  }, []);

  useEffect(function() {
    if (status === 'recording') {
      pulseLoop.current = Animated.loop(Animated.sequence([
        Animated.timing(pulseAnim, { toValue: 1.1, duration: 500, useNativeDriver: true }),
        Animated.timing(pulseAnim, { toValue: 1, duration: 500, useNativeDriver: true }),
      ]));
      pulseLoop.current.start();
    } else {
      if (pulseLoop.current) pulseLoop.current.stop();
      Animated.timing(pulseAnim, { toValue: 1, duration: 150, useNativeDriver: true }).start();
    }
  }, [status]);

  function addMessage(type, text) {
    setMessages(function(prev) { return prev.slice(-20).concat([{ type: type, text: text }]); });
  }

  async function startRecording() {
    if (status !== 'ready') return;
    try {
      await Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
      await Audio.setAudioModeAsync({ allowsRecordingIOS: true, playsInSilentModeIOS: true });
      var result = await Audio.Recording.createAsync({
        android: { extension: '.wav', sampleRate: 16000, numberOfChannels: 1, outputFormat: 2, audioEncoder: 1 },
        ios: { extension: '.wav', sampleRate: 16000, numberOfChannels: 1, outputFormat: 'lpcm', audioQuality: 127, linearPCMBitDepth: 16, linearPCMIsBigEndian: false, linearPCMIsFloat: false },
      });
      recordingRef.current = result.recording;
      setStatus('recording');
    } catch (e) {
      addMessage('system', 'Microphone not available');
    }
  }

  async function stopRecording() {
    if (status !== 'recording' || !recordingRef.current) return;
    setStatus('thinking');
    await Haptics.notificationAsync(Haptics.NotificationFeedbackType.Success);
    try {
      await recordingRef.current.stopAndUnloadAsync();
      var uri = recordingRef.current.getURI();
      recordingRef.current = null;
      var response = await fetch(uri);
      var blob = await response.blob();
      var arrayBuffer = await blob.arrayBuffer();
      var res = await sendAppVoice(arrayBuffer, robot.id, robot.mode, kidName);
      if (res.ok && res.status === 200) {
        var replyText = res.headers.get('X-Reply-Text') || '...';
        addMessage('robot', robot.name + ': ' + replyText);
        setStatus('speaking');
        var audioData = await res.arrayBuffer();
        await playResponse(audioData);
      } else {
        addMessage('system', 'No response - try again!');
      }
    } catch (e) {
      addMessage('system', 'Could not reach Cerebro');
    }
    await Audio.setAudioModeAsync({ allowsRecordingIOS: false });
    setStatus('ready');
  }

  async function playResponse(arrayBuffer) {
    try {
      var int16 = new Int16Array(arrayBuffer);
      var wav = pcmToWav(int16, 16000);
      var b64 = arrayBufferToBase64(wav);
      if (soundRef.current) await soundRef.current.unloadAsync();
      var result = await Audio.Sound.createAsync({ uri: 'data:audio/wav;base64,' + b64 }, { shouldPlay: true });
      soundRef.current = result.sound;
      await new Promise(function(resolve) {
        result.sound.setOnPlaybackStatusUpdate(function(s) { if (s.didJustFinish) resolve(); });
      });
    } catch (e) { /* playback error */ }
  }

  function pcmToWav(samples, sr) {
    var buf = new ArrayBuffer(44 + samples.length * 2);
    var v = new DataView(buf);
    function w(o, s) { for (var i = 0; i < s.length; i++) v.setUint8(o + i, s.charCodeAt(i)); }
    w(0, 'RIFF'); v.setUint32(4, 36 + samples.length * 2, true);
    w(8, 'WAVE'); w(12, 'fmt '); v.setUint32(16, 16, true); v.setUint16(20, 1, true);
    v.setUint16(22, 1, true); v.setUint32(24, sr, true); v.setUint32(28, sr * 2, true);
    v.setUint16(32, 2, true); v.setUint16(34, 16, true); w(36, 'data');
    v.setUint32(40, samples.length * 2, true);
    for (var i = 0; i < samples.length; i++) v.setInt16(44 + i * 2, samples[i], true);
    return buf;
  }

  function arrayBufferToBase64(buf) {
    var bytes = new Uint8Array(buf);
    var binary = '';
    for (var i = 0; i < bytes.length; i++) binary += String.fromCharCode(bytes[i]);
    return btoa(binary);
  }

  var isOnline = robotStatus.cerebroOnline;
  var isSleeping = robotStatus.sleeping;
  var statusColor = isOnline ? (isSleeping ? '#f59e0b' : '#4caf50') : '#ef4444';
  var statusMsg = isOnline ? (isSleeping ? robot.name + ' is sleeping' : robot.name + ' is ready') : robot.name + ' is offline';
  var btnLabel = status === 'recording' ? 'LISTENING...' : 'HOLD TO TALK';
  var statusLabel = { ready: '', recording: 'Listening...', thinking: 'Thinking...', speaking: robot.name + ' is talking...' }[status] || '';

  return (
    <View style={styles.container}>
      <View style={styles.statusBar}>
        <View style={[styles.statusDot, { backgroundColor: statusColor }]} />
        <Text style={[styles.statusText, { color: statusColor }]}>{statusMsg}</Text>
      </View>

      <View style={[styles.faceBox, { borderColor: robot.color, backgroundColor: robot.bg }]}>
        <Text style={styles.faceEmoji}>{myRobot === 'robot2' ? '\u{1F9E1}' : '\u{1F49C}'}</Text>
        <Text style={[styles.faceName, { color: robot.textColor }]}>{robot.name}</Text>
      </View>

      <ScrollView style={styles.chat} ref={scrollRef}
        onContentSizeChange={function() { if (scrollRef.current) scrollRef.current.scrollToEnd({ animated: true }); }}>
        {messages.map(function(m, i) {
          var isRobot = m.type === 'robot';
          var isSystem = m.type === 'system';
          return (
            <View key={i} style={[styles.bubble, isRobot && [styles.bubbleRobot, { backgroundColor: robot.bg }], isSystem && styles.bubbleSystem]}>
              <Text style={[styles.bubbleText, isRobot && { color: robot.textColor }, isSystem && styles.systemText]}>{m.text}</Text>
            </View>
          );
        })}
        {status === 'thinking' && <View style={styles.bubbleSystem}><ActivityIndicator size="small" color={robot.color} /></View>}
      </ScrollView>

      {statusLabel ? <Text style={[styles.statusLabel, { color: robot.color }]}>{statusLabel}</Text> : <View style={{ height: 20 }} />}

      <MicBars active={status === 'recording'} color={robot.color} />

      <Animated.View style={{ transform: [{ scale: pulseAnim }] }}>
        <Pressable
          style={[styles.talkBtn, { backgroundColor: robot.color }, status !== 'ready' && { opacity: 0.85 }]}
          onPressIn={startRecording}
          onPressOut={stopRecording}
          disabled={status === 'thinking' || status === 'speaking'}>
          <Text style={styles.talkIcon}>{"\uD83C\uDFA4"}</Text>
          <Text style={styles.talkLabel}>{btnLabel}</Text>
        </Pressable>
      </Animated.View>
    </View>
  );
}

var styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0f0f13', alignItems: 'center', padding: 16, paddingTop: 8, gap: 10 },
  statusBar: { flexDirection: 'row', alignItems: 'center', gap: 6, paddingVertical: 2 },
  statusDot: { width: 8, height: 8, borderRadius: 4 },
  statusText: { fontSize: 12, fontWeight: '700' },
  faceBox: { width: 160, height: 160, borderRadius: 28, borderWidth: 3, alignItems: 'center', justifyContent: 'center', gap: 4 },
  faceEmoji: { fontSize: 64 },
  faceName: { fontSize: 20, fontWeight: '900' },
  chat: { flex: 1, width: '100%' },
  bubble: { maxWidth: '85%', padding: 12, borderRadius: 18, marginBottom: 8, alignSelf: 'flex-end', backgroundColor: '#3b82f6' },
  bubbleRobot: { alignSelf: 'flex-start', borderBottomLeftRadius: 4 },
  bubbleSystem: { alignSelf: 'center', backgroundColor: 'rgba(255,255,255,.1)', borderRadius: 12, padding: 8 },
  bubbleText: { fontSize: 15, fontWeight: '700', lineHeight: 22, color: 'white' },
  systemText: { color: 'rgba(255,255,255,.5)', fontSize: 13 },
  statusLabel: { fontSize: 14, fontWeight: '700', height: 20 },
  bars: { flexDirection: 'row', gap: 4, height: 32, alignItems: 'center', marginBottom: 4 },
  bar: { width: 6, height: 28, borderRadius: 3, opacity: 0.8 },
  talkBtn: { width: 140, height: 140, borderRadius: 70, alignItems: 'center', justifyContent: 'center', gap: 6, elevation: 10, marginBottom: 16 },
  talkIcon: { fontSize: 44 },
  talkLabel: { color: 'white', fontWeight: '900', fontSize: 13, letterSpacing: 0.5 },
});
