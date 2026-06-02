// src/screens/KidScreen.js
import React, { useState, useRef, useEffect } from 'react';
import {
  View, Text, StyleSheet, Pressable, Animated,
  ScrollView, ActivityIndicator, Platform,
} from 'react-native';
import { Audio }        from 'expo-av';
import * as Haptics     from 'expo-haptics';
import * as KeepAwake   from 'expo-keep-awake';
import AsyncStorage     from '@react-native-async-storage/async-storage';
import { sendAudioChat, getCerebroUrl } from '../cerebroApi';

const ROBOTS = [
  { id: 'robot2', name: 'Rumi', color: '#ff6eb4', bg: '#ffe0f0', textColor: '#c2185b' },
  { id: 'robot1', name: 'Kira', color: '#8b5cf6', bg: '#ede9fe', textColor: '#5b21b6' },
];

export default function KidScreen() {
  const [selected,   setSelected]   = useState(0);   // 0 = Rumi, 1 = Kira
  const [status,     setStatus]     = useState('ready');  // ready|recording|thinking|speaking
  const [messages,   setMessages]   = useState([{ type: 'system', text: '👋 Hold the button to talk!' }]);
  const [kidName,    setKidName]    = useState('YOUR_SONS_NAME');

  const recordingRef = useRef(null);
  const soundRef     = useRef(null);
  const pulseAnim    = useRef(new Animated.Value(1)).current;
  const pulseLoop    = useRef(null);

  const robot = ROBOTS[selected];

  useEffect(() => {
    Audio.requestPermissionsAsync();
    AsyncStorage.getItem('kid_name').then(n => n && setKidName(n));
    KeepAwake.activateKeepAwakeAsync();
    return () => KeepAwake.deactivateKeepAwake();
  }, []);

  // Pulse animation when recording
  useEffect(() => {
    if (status === 'recording') {
      pulseLoop.current = Animated.loop(
        Animated.sequence([
          Animated.timing(pulseAnim, { toValue: 1.15, duration: 500, useNativeDriver: true }),
          Animated.timing(pulseAnim, { toValue: 1,    duration: 500, useNativeDriver: true }),
        ])
      );
      pulseLoop.current.start();
    } else {
      pulseLoop.current?.stop();
      Animated.timing(pulseAnim, { toValue: 1, duration: 150, useNativeDriver: true }).start();
    }
  }, [status]);

  async function startRecording() {
    if (status !== 'ready') return;
    try {
      await Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
      await Audio.setAudioModeAsync({
        allowsRecordingIOS:     true,
        playsInSilentModeIOS:   true,
        staysActiveInBackground: false,
      });
      const { recording } = await Audio.Recording.createAsync({
        android: {
          extension:   '.wav',
          outputFormat: Audio.RECORDING_OPTION_ANDROID_OUTPUT_FORMAT_DEFAULT,
          audioEncoder: Audio.RECORDING_OPTION_ANDROID_AUDIO_ENCODER_DEFAULT,
          sampleRate:   16000,
          numberOfChannels: 1,
          bitRate:      128000,
        },
        ios: {
          extension:        '.wav',
          outputFormat:     Audio.RECORDING_OPTION_IOS_OUTPUT_FORMAT_LINEARPCM,
          audioQuality:     Audio.RECORDING_OPTION_IOS_AUDIO_QUALITY_HIGH,
          sampleRate:       16000,
          numberOfChannels: 1,
          bitRateStrategy:  Audio.RECORDING_OPTION_IOS_BIT_RATE_STRATEGY_CONSTANT,
          linearPCMBitDepth: 16,
          linearPCMIsBigEndian: false,
          linearPCMIsFloat:     false,
        },
      });
      recordingRef.current = recording;
      setStatus('recording');
    } catch (e) {
      console.error('Record start error:', e);
      addMessage('system', '❌ Microphone not available');
    }
  }

  async function stopRecording() {
    if (status !== 'recording' || !recordingRef.current) return;
    setStatus('thinking');
    await Haptics.notificationAsync(Haptics.NotificationFeedbackType.Success);

    try {
      await recordingRef.current.stopAndUnloadAsync();
      const uri = recordingRef.current.getURI();
      recordingRef.current = null;

      // Fetch the audio file as a blob
      const response = await fetch(uri);
      const audioBlob = await response.blob();
      const arrayBuffer = await audioBlob.arrayBuffer();

      // Send to Cerebro
      const res = await sendAudioChat(
        arrayBuffer,
        robot.id,
        robot.id === 'robot2' ? 'kid' : 'adult',
        kidName
      );

      if (res.ok && res.status === 200) {
        const replyText = res.headers.get('X-Reply-Text') || '...';
        addMessage(robot.name.toLowerCase(), `${robot.name}: ${replyText}`);
        setStatus('speaking');

        // Play the response audio
        const audioData = await res.arrayBuffer();
        await playPCMResponse(audioData);
      } else if (res.status === 204) {
        addMessage('system', "Hmm, didn't catch that — try again!");
      } else {
        addMessage('system', '❌ Cerebro is offline');
      }
    } catch (e) {
      console.error('Send error:', e);
      addMessage('system', '❌ Could not reach Cerebro');
    }

    await Audio.setAudioModeAsync({ allowsRecordingIOS: false });
    setStatus('ready');
  }

  async function playPCMResponse(arrayBuffer) {
    try {
      const int16 = new Int16Array(arrayBuffer);
      // Convert Int16 PCM to a WAV file in memory
      const wav = pcmToWav(int16, 16000);
      const base64 = arrayBufferToBase64(wav);

      if (soundRef.current) {
        await soundRef.current.unloadAsync();
      }
      const { sound } = await Audio.Sound.createAsync(
        { uri: `data:audio/wav;base64,${base64}` },
        { shouldPlay: true }
      );
      soundRef.current = sound;
      await new Promise(resolve => {
        sound.setOnPlaybackStatusUpdate(s => {
          if (s.didJustFinish) resolve();
        });
      });
    } catch (e) {
      console.error('Playback error:', e);
    }
  }

  function pcmToWav(int16Array, sampleRate) {
    const numSamples = int16Array.length;
    const byteRate   = sampleRate * 2;
    const dataSize   = numSamples * 2;
    const buffer     = new ArrayBuffer(44 + dataSize);
    const view       = new DataView(buffer);
    const write = (offset, str) => [...str].forEach((c, i) => view.setUint8(offset + i, c.charCodeAt(0)));
    write(0,  'RIFF'); view.setUint32(4,  36 + dataSize, true);
    write(8,  'WAVE'); write(12, 'fmt ');
    view.setUint32(16, 16, true); view.setUint16(20, 1, true);
    view.setUint16(22, 1, true);  view.setUint32(24, sampleRate, true);
    view.setUint32(28, byteRate, true); view.setUint16(32, 2, true);
    view.setUint16(34, 16, true); write(36, 'data');
    view.setUint32(40, dataSize, true);
    int16Array.forEach((v, i) => view.setInt16(44 + i * 2, v, true));
    return buffer;
  }

  function arrayBufferToBase64(buffer) {
    const bytes = new Uint8Array(buffer);
    let binary  = '';
    bytes.forEach(b => binary += String.fromCharCode(b));
    return btoa(binary);
  }

  function addMessage(type, text) {
    setMessages(prev => [...prev.slice(-20), { type, text }]);
  }

  const statusLabel = {
    ready:     '',
    recording: '🔴 Listening...',
    thinking:  '💭 Thinking...',
    speaking:  `💬 ${robot.name} is talking...`,
  }[status];

  return (
    <View style={[styles.container, { backgroundColor: '#1a1030' }]}>

      {/* Robot selector */}
      <View style={styles.tabs}>
        {ROBOTS.map((r, i) => (
          <Pressable
            key={r.id}
            style={[styles.tab, selected === i && { backgroundColor: r.bg, borderColor: r.color }]}
            onPress={() => setSelected(i)}
          >
            <Text style={[styles.tabText, selected === i && { color: r.textColor }]}>
              {i === 0 ? '🩷' : '💜'} {r.name}
            </Text>
          </Pressable>
        ))}
      </View>

      {/* Robot face placeholder */}
      <View style={[styles.faceBox, { borderColor: robot.color, backgroundColor: robot.bg }]}>
        <Text style={styles.faceEmoji}>{selected === 0 ? '🤖' : '🤖'}</Text>
        <Text style={[styles.faceName, { color: robot.textColor }]}>{robot.name}</Text>
      </View>

      {/* Chat bubbles */}
      <ScrollView
        style={styles.chat}
        ref={ref => ref?.scrollToEnd({ animated: true })}
        contentContainerStyle={{ paddingBottom: 8 }}
      >
        {messages.map((m, i) => (
          <View key={i} style={[
            styles.bubble,
            m.type === 'rumi'   && styles.bubbleRumi,
            m.type === 'kira'   && styles.bubbleKira,
            m.type === 'system' && styles.bubbleSystem,
          ]}>
            <Text style={[
              styles.bubbleText,
              m.type === 'rumi'   && { color: '#c2185b' },
              m.type === 'kira'   && { color: '#5b21b6' },
              m.type === 'system' && { color: 'rgba(255,255,255,.55)', fontSize: 13 },
            ]}>
              {m.text}
            </Text>
          </View>
        ))}
        {status === 'thinking' && (
          <View style={styles.bubbleSystem}>
            <ActivityIndicator size="small" color={robot.color} />
          </View>
        )}
      </ScrollView>

      {/* Status text */}
      {statusLabel ? (
        <Text style={[styles.statusText, { color: robot.color }]}>{statusLabel}</Text>
      ) : <View style={{ height: 22 }} />}

      {/* Big hold-to-talk button */}
      <Animated.View style={{ transform: [{ scale: pulseAnim }] }}>
        <Pressable
          style={[styles.talkBtn, { backgroundColor: robot.color },
            status !== 'ready' && styles.talkBtnActive]}
          onPressIn={startRecording}
          onPressOut={stopRecording}
          disabled={status === 'thinking' || status === 'speaking'}
        >
          <Text style={styles.talkIcon}>🎤</Text>
          <Text style={styles.talkLabel}>
            {status === 'recording' ? 'LISTENING...' : 'HOLD TO TALK'}
          </Text>
        </Pressable>
      </Animated.View>

    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, alignItems: 'center', padding: 16, paddingTop: 8, gap: 12 },
  tabs: { flexDirection: 'row', gap: 10, width: '100%' },
  tab: {
    flex: 1, padding: 10, borderRadius: 14, borderWidth: 2.5,
    borderColor: 'transparent', backgroundColor: 'rgba(255,255,255,.07)',
    alignItems: 'center',
  },
  tabText: { fontWeight: '800', fontSize: 15, color: 'rgba(255,255,255,.4)' },
  faceBox: {
    width: 160, height: 160, borderRadius: 28, borderWidth: 3,
    alignItems: 'center', justifyContent: 'center', gap: 4,
  },
  faceEmoji: { fontSize: 64 },
  faceName:  { fontSize: 18, fontWeight: '900' },
  chat: { flex: 1, width: '100%' },
  bubble: {
    maxWidth: '85%', padding: 12, borderRadius: 18, marginBottom: 8,
    alignSelf: 'flex-end',
  },
  bubbleRumi:   { alignSelf: 'flex-start', backgroundColor: '#ffe0f0', borderBottomLeftRadius: 4 },
  bubbleKira:   { alignSelf: 'flex-start', backgroundColor: '#ede9fe', borderBottomLeftRadius: 4 },
  bubbleSystem: { alignSelf: 'center', backgroundColor: 'rgba(255,255,255,.1)', borderRadius: 12, padding: 8 },
  bubbleText:   { fontSize: 15, fontWeight: '700', lineHeight: 22, color: 'white' },
  statusText:   { fontSize: 14, fontWeight: '700', height: 22 },
  talkBtn: {
    width: 150, height: 150, borderRadius: 75,
    alignItems: 'center', justifyContent: 'center', gap: 6,
    shadowColor: '#000', shadowOffset: { width: 0, height: 4 },
    shadowOpacity: .4, shadowRadius: 12, elevation: 10,
    marginBottom: 16,
  },
  talkBtnActive: { opacity: .85 },
  talkIcon:  { fontSize: 48 },
  talkLabel: { color: 'white', fontWeight: '900', fontSize: 13, letterSpacing: .5 },
});
