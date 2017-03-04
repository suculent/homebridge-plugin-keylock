/*
 * This HAP device bridges BLE to defined or default mqtt broker/channel.
 */

'use strict';

var request = require('request');

var Service, Characteristic;

// should be overridden from config
var default_broker_address = 'mqtt://192.168.1.21'
var default_mqtt_channel = "/keylock/A"

var mqtt = require('mqtt')
var mqttClient = null;

module.exports = function(homebridge) {
  Service = homebridge.hap.Service;
  Characteristic = homebridge.hap.Characteristic;
  homebridge.registerAccessory("homebridge-keylock", "Keylock",
    KeylockAccessory);
}

function KeylockAccessory(log, config) {

  this.log = log;

  var that = this;

  this.name = config['name'] || "Keylock";
  this.mqttBroker = config['mqtt_broker'];
  this.mqttChannel = config['mqtt_channel'];

  this.state = Characteristic.LockTargetState.SECURED;
  this.status = 0;

  this.infoservice = new Service.AccessoryInformation();

  this.infoservice
    .setCharacteristic(Characteristic.Manufacturer, "Page 42")
    .setCharacteristic(Characteristic.Model, "Keylock")
    .setCharacteristic(Characteristic.SerialNumber, "1");

  this.lockservice = new Service.LockMechanism(this.name);

  this.lockservice
    .getCharacteristic(Characteristic.LockCurrentState)
    .on('get', this.getState.bind(this));

  this.lockservice
    .getCharacteristic(Characteristic.LockTargetState)
    .on('get', this.getState.bind(this))
    .on('set', this.setState.bind(this));

  if (!this.mqttBroker) {
    this.log.warn('Config is missing mqtt_broker, fallback to default.');
    this.mqttBroker = default_broker_address;
    if (!this.mqttBroker.contains("mqtt://")) {
      this.mqttBroker = "mqtt://" + this.mqttBroker;
    }
  }

  if (!this.mqttChannel) {
    this.log.warn('Config is missing mqtt_channel, fallback to default.');
    this.mqttChannel = default_mqtt_channel;
  }

  this.log("Connecting to mqtt broker: " + this.mqttBroker)
  mqttClient = mqtt.connect(this.mqttBroker)

  mqttClient.on('connect', function() {
    var subscription = that.mqttChannel
    that.log("MQTT connected, subscribing to monitor: " + subscription)
    mqttClient.subscribe(subscription)
  })

  mqttClient.on('error', function() {
    that.log("MQTT error")
    this.status = -1
  })

  mqttClient.on('offline', function() {
    that.log("MQTT offline")
    this.status = -1
  })

  mqttClient.on('message', function(topic, message) {

    var m_string = message.toString();

    if ((m_string == "UNLOCK") || (m_string == "LOCK")) {
      return;
    }

    if (m_string == "DISCONNECTED") {
      return;
    }

    //that.log("topic: " + topic.toString())
    that.log("message: " + m_string)

    var m = JSON.parse(message);

    var status = m.status.toString();
    var state;

    that.log("status: " + status);

    if (status == "locking") {
      state = Characteristic.LockCurrentState.UNSECURED;
    }

    if (status == "locked") {
      state = Characteristic.LockCurrentState.SECURED;
    }

    if (status == "unlocking") {
      state = Characteristic.LockCurrentState.UNSECURED;
    }

    if (status == "unlocked") {
      state = Characteristic.LockCurrentState.UNSECURED;
    }

    // only on /home channel
    if (status == "connected") {
      state = Characteristic.LockCurrentState.SECURED;
    }

    // that.log("Updating state to: " + state);

    that.lockservice
      .setCharacteristic(Characteristic.LockCurrentState, state);

    // {"identifier":"keylock_A","action":"unlock", "millis":"50824", "angle":"93"}
  })
}

// listen for the "identify" event for this Accessory
KeylockAccessory.prototype.identify = function(paired, callback) {
  this.log("Identify.");
  callback(); // success
};

KeylockAccessory.prototype.getState = function(callback) {
  if (this.state == -1) {
    this.log("getState:ERR");
    callback(err);
  } else {
    this.log("getState:" + this.state);
    callback(null, this.state); // success
  }
}

KeylockAccessory.prototype.setState = function(state, callback) {
  if (state == Characteristic.LockTargetState.SECURED) {
    mqttClient.publish(this.mqttChannel, "LOCK");
  } else {
    mqttClient.publish(this.mqttChannel, "UNLOCK");
  }
  callback(null); // success
}

KeylockAccessory.prototype.getServices = function() {
  return [this.lockservice, this.infoservice];
}
