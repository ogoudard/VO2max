//
//  BluetoothManager.swift
//  VO2
//
//  Created by Goudard Olivier on 07/11/2024.
//

import Foundation
import CoreBluetooth
import SwiftUI

class BluetoothManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    var characteristic: CBCharacteristic?
    @Published var historicalData: [DataEntry] = []
    @Published var vo2Maxvalue: Float = 0 // Valeur affichée mise à jour
    @Published var vo2Maxvalues: [(time: String, value: Float)] = [
        (time: "08:00", value: 32.0),
        (time: "08:05", value: 34.5),
        (time: "08:10", value: 35.2),
        (time: "08:15", value: 37.8),
        (time: "08:20", value: 38.1),
        (time: "08:25", value: 40.4),
        (time: "08:30", value: 42.7),
        (time: "08:35", value: 45.2),
        (time: "08:40", value: 46.5),
        (time: "08:45", value: 47.9),
        (time: "08:50", value: 49.3),
        (time: "08:55", value: 50.6),
        (time: "09:00", value: 51.8) // Valeurs VO2max pour le graphique
        ]
    @Published var vo2value: Float = 0 // Valeur affichée mise à jour
    @Published var vo2values: [(time: String, value: Float)] = [
        (time: "08:00", value: 32.0),
        (time: "08:05", value: 34.5),
        (time: "08:10", value: 35.2),
        (time: "08:15", value: 37.8),
        (time: "08:20", value: 38.1),
        (time: "08:25", value: 40.4),
        (time: "08:30", value: 42.7),
        (time: "08:35", value: 45.2),
        (time: "08:40", value: 46.5),
        (time: "08:45", value: 47.9),
        (time: "08:50", value: 49.3),
        (time: "08:55", value: 50.6),
        (time: "09:00", value: 51.8) // Valeurs VO2max pour le graphique

    ] // Valeurs VO2max pour le graphique
    @Published var vco2value: Float = 0 // Valeur affichée mise à jour
    @Published var vco2values: [(time: String, value: Float)] = [
        (time: "08:00", value: 32.0),
        (time: "08:05", value: 34.5),
        (time: "08:10", value: 35.2),
        (time: "08:15", value: 37.8),
        (time: "08:20", value: 38.1),
        (time: "08:25", value: 40.4),
        (time: "08:30", value: 42.7),
        (time: "08:35", value: 45.2),
        (time: "08:40", value: 46.5),
        (time: "08:45", value: 47.9),
        (time: "08:50", value: 49.3),
        (time: "08:55", value: 50.6),
        (time: "09:00", value: 51.8) // Valeurs VO2max pour le graphique

    ] // Valeurs VO2max pour le graphique
    @Published var RQvalue: Float = 0 // Valeur affichée mise à jour
    @Published var RQvalues: [(time: String, value: Float)] = [
        (time: "08:00", value: 32.0),
        (time: "08:05", value: 34.5),
        (time: "08:10", value: 35.2),
        (time: "08:15", value: 37.8),
        (time: "08:20", value: 38.1),
        (time: "08:25", value: 40.4),
        (time: "08:30", value: 42.7),
        (time: "08:35", value: 45.2),
        (time: "08:40", value: 46.5),
        (time: "08:45", value: 47.9),
        (time: "08:50", value: 49.3),
        (time: "08:55", value: 50.6),
        (time: "09:00", value: 51.8) // Valeurs VO2max pour le graphique

    ] // Valeurs VO2max pour le graphique
    @Published var o2percvalue: Float = 0 // Valeur affichée mise à jour
    @Published var o2percvalues: [(time: String, value: Float)] = [] // Valeurs VO2max pour le graphique
    @Published var co2percvalue: Float = 0 // Valeur affichée mise à jour
    @Published var co2percvalues: [(time: String, value: Float)] = [] // Valeurs VO2max pour le graphique
    @Published var energyvalue: Float = 0 // Valeur affichée mise à jour
    @Published var energyvalues: [(time: String, value: Float)] = [] // Valeurs VO2max pour le graphique
    @Published var connectionColor: Color = .red  // Rouge par défaut (déconnecté)
    
    var centralManager: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var vo2MaxCharacteristic: CBCharacteristic?
    private var vo2Characteristic: CBCharacteristic?
    private var vco2Characteristic: CBCharacteristic?
    private var RQCharacteristic: CBCharacteristic?
    private var o2percCharacteristic: CBCharacteristic?
    private var co2percCharacteristic: CBCharacteristic?
    private var energyCharacteristic: CBCharacteristic?
    private var startTime: Date = Date() // Pour calculer le temps
    
    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }
    
    func startBluetooth() {
        print("🟢 Début du scan Bluetooth")
        centralManager.delegate = self
        centralManager.scanForPeripherals(withServices: [CBUUID(string: "b354cf1b-2486-4f21-b4b1-ee4cd5cc3bf0")], options: nil)
    }
    
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        print("🟡 État du Bluetooth: \(central.state.rawValue)")
        if central.state == .poweredOn {
            print("🟢 Bluetooth activé, début du scan")
            centralManager.scanForPeripherals(withServices: [CBUUID(string: "b354cf1b-2486-4f21-b4b1-ee4cd5cc3bf0")], options: nil)
            //centralManager.scanForPeripherals(withServices: [CBUUID(string: "4225d51b-f1c2-419a-9acc-bd8e70d960ae")], options: nil)
        } else {
            print("🔴 Bluetooth non disponible")
        }
        switch central.state {
        case .poweredOn:
            connectionColor = .blue // En cours de connexion
        case .poweredOff, .unsupported, .unauthorized, .resetting:
            connectionColor = .red // Bluetooth indisponible
        case .unknown:
            break
        @unknown default:
            break
        }
        
        
    }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        print("🟢 Périphérique découvert: \(peripheral.name ?? "Inconnu")")
        self.peripheral = peripheral
        self.peripheral?.delegate = self
        centralManager.stopScan()
        print("🔵 Connexion au périphérique")
        centralManager.connect(peripheral, options: nil)
    }
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("🟢 Connecté au périphérique: \(peripheral.name ?? "Inconnu")")
        peripheral.discoverServices([CBUUID(string: "b354cf1b-2486-4f21-b4b1-ee4cd5cc3bf0")])
        peripheral.discoverServices([CBUUID(string: "231c616b-32a6-4b93-9f0c-fe728deca0a5")])
        peripheral.discoverServices([CBUUID(string: "196c1c76-fe53-47e7-baab-a32b404d63df")])
        peripheral.discoverServices([CBUUID(string: "8868ba7f-cada-4035-85d2-e0002aeb2be6")])
        peripheral.discoverServices([CBUUID(string: "f362445d-0824-40e1-a53a-8e90106f0ba3")])
        peripheral.discoverServices([CBUUID(string: "4b1ad6ec-d8d3-426c-8d1e-0dea4d6b4ebc")])
        peripheral.discoverServices([CBUUID(string: "3483923e-e94a-4a21-96c4-c12a442ee4cf")])
        connectionColor = .green // Connecté
        
    }
    
    
    
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error = error {
            print("🔴 Erreur de découverte des services: \(error)")
            return
        }
        
        print("🟢 Services découverts")
        for service in peripheral.services ?? [] {
            print("🔵 Découverte des caractéristiques pour le service \(service.uuid)")
            peripheral.discoverCharacteristics([CBUUID(string: "c70c73fd-b5fb-4a5f-87f4-7a187590b0ef")], for: service)
            peripheral.discoverCharacteristics([CBUUID(string: "4225d51b-f1c2-419a-9acc-bd8e70d960ae")], for: service)
            peripheral.discoverCharacteristics([CBUUID(string: "a4534c9e-0ede-48ec-bee5-7b728ecb25df")], for: service)
            peripheral.discoverCharacteristics([CBUUID(string: "b192eef4-a298-43fe-b85c-b04d934448f7")], for: service)
            peripheral.discoverCharacteristics([CBUUID(string: "f1329c52-97b8-4945-a8a2-2127426d71ba")], for: service)
            peripheral.discoverCharacteristics([CBUUID(string: "86c8ca0e-885f-4987-82ba-a794574cc6cf")], for: service)
            peripheral.discoverCharacteristics([CBUUID(string: "fb6fc35b-2896-4474-8647-bd206b3d0c50")], for: service)
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let error = error {
            print("🔴 Erreur de découverte des caractéristiques: \(error)")
            return
        }
        
        for characteristic in service.characteristics ?? [] {
            if characteristic.uuid == CBUUID(string: "c70c73fd-b5fb-4a5f-87f4-7a187590b0ef") {
                vo2MaxCharacteristic = characteristic
                print("🔵 Enregistrement de notifications pour la caractéristique VO2Max")
                peripheral.setNotifyValue(true, for: characteristic)
            }
            if characteristic.uuid == CBUUID(string: "4225d51b-f1c2-419a-9acc-bd8e70d960ae") {
                vo2Characteristic = characteristic
                print("🔵 Enregistrement de notifications pour la caractéristique VO2")
                peripheral.setNotifyValue(true, for: characteristic)
            }
            if characteristic.uuid == CBUUID(string: "a4534c9e-0ede-48ec-bee5-7b728ecb25df") {
                vco2Characteristic = characteristic
                print("🔵 Enregistrement de notifications pour la caractéristique VCO2")
                peripheral.setNotifyValue(true, for: characteristic)
            }
            if characteristic.uuid == CBUUID(string: "b192eef4-a298-43fe-b85c-b04d934448f7") {
                RQCharacteristic = characteristic
                print("🔵 Enregistrement de notifications pour la caractéristique RQ")
                peripheral.setNotifyValue(true, for: characteristic)
            }
            if characteristic.uuid == CBUUID(string: "f1329c52-97b8-4945-a8a2-2127426d71ba") {
                o2percCharacteristic = characteristic
                print("🔵 Enregistrement de notifications pour la caractéristique O2perc")
                peripheral.setNotifyValue(true, for: characteristic)
            }
            if characteristic.uuid == CBUUID(string: "86c8ca0e-885f-4987-82ba-a794574cc6cf") {
                co2percCharacteristic = characteristic
                print("🔵 Enregistrement de notifications pour la caractéristique CO2")
                peripheral.setNotifyValue(true, for: characteristic)
            }
            if characteristic.uuid == CBUUID(string: "fb6fc35b-2896-4474-8647-bd206b3d0c50") {
                energyCharacteristic = characteristic
                print("🔵 Enregistrement de notifications pour la caractéristique energy")
                peripheral.setNotifyValue(true, for: characteristic)
            }
            
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard error == nil else {
            print("Erreur de mise à jour de la caractéristique: \(error!)")
            return
        }
        if  characteristic.uuid == CBUUID(string: "c70c73fd-b5fb-4a5f-87f4-7a187590b0ef"){
            if let data = characteristic.value {
                print("🟢 Données reçues: \(data)")
                vo2Maxvalue = parseData(data)
                print("🟢 Nouvelle valeur VO2max : \(vo2Maxvalue)")
                // Ajoutez la nouvelle valeur à la liste
                let elapsedTime = Date().timeIntervalSince(startTime)
                let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                vo2Maxvalues.append((time: timeString, value: vo2Maxvalue))
                
            }
            
        }
        if  characteristic.uuid == CBUUID(string: "4225d51b-f1c2-419a-9acc-bd8e70d960ae"){
            if let data = characteristic.value {
                print("🟢 Données reçues: \(data)")
                vo2value = parseDatavo2(data)
                print("🟢 Nouvelle valeur VO2 : \(vo2value)")
                // Ajoutez la nouvelle valeur à la liste
                let elapsedTime = Date().timeIntervalSince(startTime)
                let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                vo2values.append((time: timeString, value: vo2value))
                
            }
            
        }
        if  characteristic.uuid == CBUUID(string: "a4534c9e-0ede-48ec-bee5-7b728ecb25df"){
            if let data = characteristic.value {
                print("🟢 Données reçues: \(data)")
                vco2value = parseData(data)
                print("🟢 Nouvelle valeur VCO2max : \(vco2value)")
                // Ajoutez la nouvelle valeur à la liste
                let elapsedTime = Date().timeIntervalSince(startTime)
                let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                vco2values.append((time: timeString, value: vco2value))
                
            }
            
        }
        if  characteristic.uuid == CBUUID(string: "b192eef4-a298-43fe-b85c-b04d934448f7"){
            if let data = characteristic.value {
                print("🟢 Données reçues: \(data)")
                RQvalue = parseData(data)
                print("🟢 Nouvelle valeur RQ : \(RQvalue)")
                // Ajoutez la nouvelle valeur à la liste
                let elapsedTime = Date().timeIntervalSince(startTime)
                let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                RQvalues.append((time: timeString, value: RQvalue))
                
            }
            
        }
        if  characteristic.uuid == CBUUID(string: "f1329c52-97b8-4945-a8a2-2127426d71ba"){
            if let data = characteristic.value {
                print("🟢 Données reçues: \(data)")
                o2percvalue = parseData(data)
                print("🟢 Nouvelle valeur O2% : \(o2percvalue)")
                // Ajoutez la nouvelle valeur à la liste
                let elapsedTime = Date().timeIntervalSince(startTime)
                let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                o2percvalues.append((time: timeString, value: o2percvalue))
                
            }
            
        }
        if  characteristic.uuid == CBUUID(string: "86c8ca0e-885f-4987-82ba-a794574cc6cf"){
            if let data = characteristic.value {
                print("🟢 Données reçues: \(data)")
                co2percvalue = parseData(data)
                print("🟢 Nouvelle valeur co2% : \(co2percvalue)")
                // Ajoutez la nouvelle valeur à la liste
                let elapsedTime = Date().timeIntervalSince(startTime)
                let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                co2percvalues.append((time: timeString, value: co2percvalue))
                
            }
            
        }
        if  characteristic.uuid == CBUUID(string: "fb6fc35b-2896-4474-8647-bd206b3d0c50"){
            if let data = characteristic.value {
                print("🟢 Données reçues: \(data)")
                energyvalue = parseData(data)
                print("🟢 Nouvelle valeur energy : \(energyvalue)")
                // Ajoutez la nouvelle valeur à la liste
                let elapsedTime = Date().timeIntervalSince(startTime)
                let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                energyvalues.append((time: timeString, value: energyvalue))
                
            }
            
        }
        
        
        
        
        
    }
    
    
    func parseData(_ data: Data) -> Float {
        guard data.count == 4 else {
            print("Erreur : Taille des données incorrecte")
            return 0.0
        }
        let floatValue = data.withUnsafeBytes { $0.load(as: Float.self) }
        print("Valeur VO2max reçue : \(floatValue)")
        return floatValue
    }
    
    func parseDatavo2(_ data: Data) -> Float {
        guard data.count == 4 else {
            print("Erreur : Taille des données incorrecte")
            return 0.0
        }
        let floatValue2 = data.withUnsafeBytes { $0.load(as: Float.self) }
        print("Valeur VO2 reçue : \(floatValue2)")
        return floatValue2
    }
    
    
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        connectionColor = .red // Déconnecté
    }
    
    
    func writeValue(_ value: UInt8) {
        guard let peripheral = peripheral, let characteristic = characteristic else {
            print("Peripheral or characteristic not found")
            return
        }
        let data = Data([value]) // Convertir la valeur 0 ou 1 en Data
        peripheral.writeValue(data, for: characteristic, type: .withResponse)
    }
 
    struct DataEntry: Identifiable {
        let id = UUID()
        let time: Date
        let vo2Max: Float
        let vo2: Float
        let vco2: Float
    }

    func updateHistoricalData() {
        let newEntry = DataEntry(time: Date(), vo2Max: vo2Maxvalue, vo2: vo2value, vco2: vco2value)
        historicalData.append(newEntry)

        // Optionnel : Limiter la taille de l'historique pour éviter trop de données
        if historicalData.count > 100 {
            historicalData.removeFirst()
        }
    }
    private var timer: Timer?

    func startUpdating() {
        timer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            self?.updateHistoricalData()
        }
    }

    func stopUpdating() {
        timer?.invalidate()
        timer = nil
    }

}



