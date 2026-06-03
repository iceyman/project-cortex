import React, { useState, useEffect, useRef } from 'react';
import { View, Text, StyleSheet, Pressable, Animated, Modal } from 'react-native';
import AsyncStorage from '@react-native-async-storage/async-storage';

var DEFAULT_PIN = '1234';

export default function PinLock(props) {
  var visible = props.visible;
  var onUnlock = props.onUnlock;
  var onCancel = props.onCancel;

  var _e = useState('');
  var entered = _e[0];
  var setEntered = _e[1];

  var _p = useState(DEFAULT_PIN);
  var savedPin = _p[0];
  var setSavedPin = _p[1];

  var _s = useState(false);
  var setting = _s[0];
  var setSetting = _s[1];

  var _n = useState('');
  var newPin = _n[0];
  var setNewPin = _n[1];

  var shakeAnim = useRef(new Animated.Value(0)).current;

  useEffect(function() {
    AsyncStorage.getItem('parent_pin').then(function(p) {
      if (p) { setSavedPin(p); }
      else { setSetting(true); }
    });
  }, [visible]);

  useEffect(function() {
    if (entered.length !== 4) return;

    if (setting) {
      if (newPin === '') {
        setNewPin(entered);
        setEntered('');
      } else {
        if (entered === newPin) {
          AsyncStorage.setItem('parent_pin', entered);
          setSavedPin(entered);
          setSetting(false);
          setNewPin('');
          setEntered('');
          onUnlock();
        } else {
          doShake();
          setNewPin('');
        }
      }
    } else {
      if (entered === savedPin) {
        setEntered('');
        onUnlock();
      } else {
        doShake();
      }
    }
  }, [entered]);

  function doShake() {
    setEntered('');
    Animated.sequence([
      Animated.timing(shakeAnim, { toValue: 10, duration: 50, useNativeDriver: true }),
      Animated.timing(shakeAnim, { toValue: -10, duration: 50, useNativeDriver: true }),
      Animated.timing(shakeAnim, { toValue: 10, duration: 50, useNativeDriver: true }),
      Animated.timing(shakeAnim, { toValue: 0, duration: 50, useNativeDriver: true }),
    ]).start();
  }

  function press(n) {
    if (entered.length < 4) setEntered(entered + n);
  }

  function del() {
    setEntered(entered.slice(0, -1));
  }

  var title = setting ? (newPin === '' ? 'Set a Parent PIN' : 'Confirm your PIN') : 'Parent Access';
  var sub = setting ? (newPin === '' ? 'Choose a 4-digit PIN' : 'Enter the same PIN again') : 'Enter your PIN to continue';

  var dots = [0, 1, 2, 3].map(function(i) {
    return React.createElement(View, {
      key: i,
      style: [styles.dot, entered.length > i ? styles.dotFilled : null]
    });
  });

  var rows = [['1','2','3'],['4','5','6'],['7','8','9'],['','0','del']];
  var keypad = rows.map(function(row, ri) {
    var keys = row.map(function(n, ni) {
      return React.createElement(Pressable, {
        key: ni,
        style: [styles.key, n === '' ? styles.keyEmpty : null],
        onPress: function() {
          if (n === 'del') del();
          else if (n !== '') press(n);
        }
      }, React.createElement(Text, {
        style: [styles.keyText, n === 'del' ? { fontSize: 18 } : null]
      }, n === 'del' ? 'DEL' : n));
    });
    return React.createElement(View, { key: ri, style: styles.row }, keys);
  });

  return React.createElement(Modal, { visible: visible, transparent: true, animationType: 'fade' },
    React.createElement(View, { style: styles.overlay },
      React.createElement(Animated.View, { style: [styles.box, { transform: [{ translateX: shakeAnim }] }] },
        React.createElement(Text, { style: styles.title }, title),
        React.createElement(Text, { style: styles.sub }, sub),
        React.createElement(View, { style: styles.dots }, dots),
        React.createElement(View, { style: styles.pad }, keypad),
        onCancel ? React.createElement(Pressable, { onPress: onCancel, style: styles.cancel },
          React.createElement(Text, { style: styles.cancelText }, 'Cancel')
        ) : null
      )
    )
  );
}

var styles = StyleSheet.create({
  overlay: { flex: 1, backgroundColor: 'rgba(0,0,0,.85)', alignItems: 'center', justifyContent: 'center' },
  box: { backgroundColor: '#1a1a24', borderRadius: 24, padding: 28, width: 320, alignItems: 'center', borderWidth: 1, borderColor: '#252535', gap: 16 },
  title: { fontSize: 20, fontWeight: '800', color: 'white' },
  sub: { fontSize: 13, color: '#555', textAlign: 'center' },
  dots: { flexDirection: 'row', gap: 16, marginVertical: 8 },
  dot: { width: 18, height: 18, borderRadius: 9, borderWidth: 2, borderColor: '#555', backgroundColor: 'transparent' },
  dotFilled: { backgroundColor: '#8b5cf6', borderColor: '#8b5cf6' },
  pad: { gap: 10, width: '100%' },
  row: { flexDirection: 'row', justifyContent: 'center', gap: 10 },
  key: { width: 80, height: 60, borderRadius: 14, backgroundColor: '#111118', alignItems: 'center', justifyContent: 'center', borderWidth: 1, borderColor: '#222232' },
  keyEmpty: { backgroundColor: 'transparent', borderColor: 'transparent' },
  keyText: { fontSize: 24, fontWeight: '700', color: 'white' },
  cancel: { marginTop: 4 },
  cancelText: { color: '#555', fontSize: 14, fontWeight: '600' },
});
