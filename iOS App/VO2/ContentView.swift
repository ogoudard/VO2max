
import SwiftUI
import Charts
import Foundation
import CoreBluetooth
import UniformTypeIdentifiers
import UIKit
import AVFoundation


struct ContentView: View {
    private let chartHeight: CGFloat = 200
    @StateObject private var locationManager = LocationManager()
    @State private var navigateToSpeedAndWeight = false // Variable d'état pour gérer la navigation
    @EnvironmentObject var intervalManager: IntervalManager
    @EnvironmentObject var bluetoothManager: BluetoothManager
    @EnvironmentObject var hrbtManager: HRBTManager
    @State private var isTimerActive = false // Indicateur si le timer est actif
    @State private var timer: Timer? = nil // Référence au timer
    @State private var timeElapsed = 0 // Temps écoulé en secondes
    @State private var intervalNumber = 1 // Numéro d'intervalle
    @State private var hasButtonBeenPressed = false // Booléen global pour suivre l'état du bouton
   @State private var timerRunning: Bool = false

    // Récupérer l'intervalle courant    sur intervalNumber
    var currentInterval: Interval? {
        guard intervalNumber > 0, intervalNumber <= intervalManager.intervals.count else { return nil }
        return intervalManager.intervals[intervalNumber - 1]
    }
/*    // Cette fonction initialise les sons
    func loadSounds() {
        if let beepURL = Bundle.main.url(forResource: "beep", withExtension: "wav"),
           let highBeepURL = Bundle.main.url(forResource: "highBeep", withExtension: "wav") {
            beepSound = beepURL
            highBeepSound = highBeepURL
        }
    }

    // Cette fonction joue un bip
    func playSound(_ sound: URL) {
        do {
            audioPlayer = try AVAudioPlayer(contentsOf: sound)
            audioPlayer?.play()
        } catch {
            print("Erreur de lecture du son : \(error)")
        }
    }

    // Appelée chaque seconde pour mettre à jour le timer
    func updateTimer() {
        if timeElapsed % 60 == 55 { // À 5 secondes avant la fin de la minute
            for _ in 0..<4 { // Émettre 4 bips toutes les secondes
                DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
                    if let beepSound = beepSound {
                        playSound(beepSound)
                    }
                }
            }
        }

        if timeElapsed % 60 == 0 && timeElapsed != 0 { // Quand on change d'intervalle (nouvelle minute)
            if let highBeepSound = highBeepSound {
                playSound(highBeepSound)
            }
        }
    }
*/
    var body: some View {
        NavigationView {
            VStack(spacing: 1) { // Ajustez le spacing si nécessaire
                HStack(spacing: 20) {
                    NavigationLink(destination: SettingsView(bluetoothManager: bluetoothManager, hrbtManager: hrbtManager)){
                        HStack {
                            Image(systemName: "gearshape.fill")
                            Text("Settings")
                                .font(.headline)
                        }
                    }
                    .padding()
                    .frame(width: 150) // Largeur fixe pour les boutons
                    .foregroundColor(.white)
                    .background(Color.blue)
                    .cornerRadius(8)
                    
                    Button(action: {
                        print("Bouton Golden Lungs appuyé")
                    }) {
                        HStack {
                            Image(systemName: "lungs.fill")
                            Text("Gold Lung")
                                .font(.headline)
                        }
                    }
                    .padding()
                    .frame(width: 150) // Largeur fixe pour les boutons                    .background(Color.blue)
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(8)
                }
                
                HStack(spacing: 20) {
                    Button(action: exportCSV) {
                        HStack {
                            Image(systemName: "doc.text.fill")
                            Text("Export Data")
                                .font(.headline)
                        }
                    }
                    .padding()
                    .frame(width: 150) // Largeur fixe pour les boutons                    .background(Color.blue)
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(8)
                    
                    NavigationLink(destination: SpeedAndWeightView()){
                        HStack {
                            Image(systemName: "figure.run")
                            Text("Test Setup")
                                .font(.headline)
                        }
                    }
                    .padding()
                    .frame(width: 150) // Largeur fixe pour les boutons                    .background(Color.blue)
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(8)
                    
                }
                .padding()
                HStack(spacing: 20) {
                    VStack(alignment: .leading, spacing: 8) {
                        Text("VO2Max:\(String(format: "%.1f", bluetoothManager.vo2Maxvalue))mL/min/kg")
                            .font(.headline)
                            .padding(.bottom, 2)
                        
                        Text("VO2Live:\(String(format: "%.1f", bluetoothManager.vo2value))mL/min/kg")
                            .font(.headline)
                            .padding(.bottom, 2)
                        
                        /* Text("VCO2 Live = \(String(format: "%.2f", bluetoothManager.vco2value)) mL/min")
                         .font(.headline)
                         .padding(.bottom, 2)
                         
                         Text("RQ Level = \(String(format: "%.2f", bluetoothManager.RQvalue)) %")
                         .font(.headline)
                         .padding(.bottom, 2)
                         */
                        Text("Timer: \(timeElapsed) seconds")
                            .font(.headline)
                            .padding(.bottom, 2)
                        
                        if let currentInterval = currentInterval {
                            Text("Speed Set: \(String(format: "%.1f", currentInterval.speed)) km/h")
                                .font(.headline)
                                .padding(.bottom, 2)
                        } else {
                            Text("Interval not set")
                                .font(.headline)
                                .padding(.bottom, 2)
                        }
                        List(intervalManager.intervals) { interval in
                            HStack {
                                Text("\(interval.time) min")
                                Spacer()
                                Text("\(String(format: "%.1f", interval.speed)) km/h")
                            }
                        }
                        
                        Button(action: {
                            if !hasButtonBeenPressed { // Vérifie si le bouton n'a pas déjà été pressé
                                hasButtonBeenPressed = true // Marque le bouton comme pressé
                                startTimer() // Démarrer le timer
                                bluetoothManager.updateHistoricalData()
                                bluetoothManager.startUpdating()
                                locationManager.startUpdatingLocation()
                                //hrbtManager.startUpdating
                            }
                        }) {
                            Text("Start Test")
                                .padding()
                                .background(hasButtonBeenPressed ? Color.gray : Color.green) // Grise le bouton après avoir été pressé
                                .foregroundColor(.white)
                                .cornerRadius(8)
                                .frame(width: 150) // Largeur fixe pour les boutons
                        }
                        .disabled(hasButtonBeenPressed) // Désactive le bouton après le premier appui
                        
                        
                        
                        
                        
                    }
                    
                    VStack(alignment: .leading, spacing: 8) {
                        Text("O2 = \(String(format: "%.2f", bluetoothManager.o2percvalue)) %")
                            .font(.headline)
                            .padding(.bottom, 2)
                        
                        Text("CO2 = \(String(format: "%.2f", bluetoothManager.co2percvalue)) %")
                            .font(.headline)
                            .padding(.bottom, 2)
                        
                        Text("Heart Rate: \(hrbtManager.heartRate)")
                            .padding(.bottom, 2)
                            .font(.headline)
                        
                        Text("Speed : \(locationManager.speed * 3.6, specifier: "%.2f") km/h")
                            .foregroundColor({
                                if let currentInterval = currentInterval {
                                    return locationManager.speed * 3.6 < currentInterval.speed ? .red : .green
                                } else {
                                    return .gray // Par défaut si currentInterval est nil
                                }
                            }())
                            .font(.headline)
                            .padding(.bottom, 2)
                            .onAppear {
                                if let interval = currentInterval {
                                    print("LocationManager Speed: \(locationManager.speed)")
                                    print("Current Interval Speed: \(interval.speed)")
                                } else {
                                    print("Current Interval is nil")
                                }
                            }
                        
                        
                        Button(action: {
                            if hasButtonBeenPressed { // Vérifie si le bouton n'a pas déjà été pressé
                                hasButtonBeenPressed = false // Marque le bouton comme pressé
                                stopTimer() // Arrêter le timer
                                bluetoothManager.updateHistoricalData()
                                bluetoothManager.stopUpdating()
                                locationManager.stopUpdatingLocation()
                            }
                        }) {
                            Text("Stop Test")
                                .padding()
                                .background(!hasButtonBeenPressed ? Color.gray : Color.green) // Grise le bouton après avoir été pressé
                                .background(Color.red)
                                .foregroundColor(.white)
                                .cornerRadius(8)
                                .frame(width: 150) // Largeur fixe pour les boutons
                        }
                        .disabled(!hasButtonBeenPressed) // Désactive le bouton après le premier appui
                        
                    }
                }
                .padding() // Ajoute un peu d'espace autour
                
                List(bluetoothManager.historicalData) { entry in
                    Text("Time: \(entry.time), VO2Max: \(entry.vo2Max), VO2: \(entry.vo2), VCO2: \(entry.vco2)")
                }
                Text("VO2 Data Evolution")
                    .font(.title2)
                    .padding(.bottom, 2)
                
                Chart {
                    // Courbe VO2Max
                    ForEach(bluetoothManager.historicalData, id: \.time) { entry in
                        LineMark(
                            x: .value("Time", entry.time),
                            y: .value("VO2Max", entry.vo2Max)
                        )
                        .foregroundStyle(by: .value("Type", "VO2Max")) // Distinction par type
                        .lineStyle(StrokeStyle(lineWidth: 2))
                        .interpolationMethod(.catmullRom) // Lissage
                    }
                    
                    // Courbe VO2
                    ForEach(bluetoothManager.historicalData, id: \.time) { entry in
                        LineMark(
                            x: .value("Time", entry.time),
                            y: .value("VO2", entry.vo2)
                        )
                        .foregroundStyle(by: .value("Type", "VO2")) // Distinction par type
                        .lineStyle(StrokeStyle(lineWidth: 2))
                        .interpolationMethod(.catmullRom) // Lissage
                    }
                    
                    // Courbe VCO2
                    ForEach(bluetoothManager.historicalData, id: \.time) { entry in
                        LineMark(
                            x: .value("Time", entry.time),
                            y: .value("VCO2", entry.vco2)
                        )
                        .foregroundStyle(by: .value("Type", "VCO2")) // Distinction par type
                        .lineStyle(StrokeStyle(lineWidth: 2))
                        .interpolationMethod(.catmullRom) // Lissage
                    }
                    
                    // Courbe Speed (provenant de LocationManager)
                    ForEach(locationManager.historicalData, id: \.time) { entry in
                        LineMark(
                            x: .value("Time", entry.time),
                            y: .value("Speed", entry.speed) // Récupère la vitesse à partir de LocationManager
                        )
                        .foregroundStyle(by: .value("Type", "Speed"))
                        .lineStyle(StrokeStyle(lineWidth: 2))
                        .interpolationMethod(.catmullRom) // Lissage
                    }
                    // Courbe Heart Rate (provenant de hrbtManager)
                    ForEach(hrbtManager.historicalData, id: \.time) { entry in
                        LineMark(
                            x: .value("Time", entry.time),
                            y: .value("Heart Rate", entry.heartRate) // Récupère le rythme cardiaque à partir de hrbtManager
                        )
                        .foregroundStyle(by: .value("Type", "Heart Rate"))
                        .lineStyle(StrokeStyle(lineWidth: 2))
                        .interpolationMethod(.catmullRom) // Lissage
                    }
                    
                }
                .frame(height: 300)
                .padding()
                
                /*.chartYAxis {
                 AxisMarks(values: .automatic) // Axe Y principal pour VO2Max, VO2, VCO2
                 }
                 .chartYAxis(id: "speed") { // Axe Y secondaire pour la vitesse
                 AxisMarks(values: .stride(by: 5)) // Ajuste l'échelle de la vitesse (par exemple, de 0 à 25 km/h)
                 }
                 .chartOverlay { proxy in
                 GeometryReader { geometry in
                 // Positionner l'axe secondaire à droite pour la vitesse
                 proxy.secondaryYAxis
                 .padding(.horizontal)
                 .offset(x: geometry.size.width * 0.8) // Décalage de l'axe secondaire à droite
                 }
                 }*/
                
                //.frame(height: 75) // Ajuste la taille du graphique si nécessaire
                
                //.background(Color.white) // Ajoutez une couleur de fond pour mieux visualiser le graphique
                //.border(Color.gray) // Optionnel : ajoutez une bordure pour la visibilité
            }
            // Navigation vers la page de saisie de vitesse et poids
             //.background(
             //    NavigationLink(destination: SpeedAndWeightView(), isActive: $navigateToSpeedAndWeight) {
              //       EmptyView()
             //    }
             //)


            .toolbar {
                ToolbarItem(placement: .principal) {
                    Text("VO₂ Max")
                        //.font(.custom("Johmuria", size: 28)) // Utilisation de la police personnalisée
                        .font(.system(size: 34, weight: .bold, design: .serif)) // Utilise Georgia
                        .foregroundColor(Color.white) // Couleur énergique
                        .fontWeight(.bold) // Gras pour un style élégant
                        .baselineOffset(4) // Ajustez cette valeur pour déplacer le "₂" vers le bas ou vers le haut
                }
                
                
                ToolbarItem(placement: .navigationBarTrailing) {
                    Text("Bluetooth")
                        .font(.caption)
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    // Rond indicateur de connexion
                    Circle()
                        .fill(bluetoothManager.connectionColor)
                        .frame(width: 20, height: 20)
                }
                
            }
            .padding(.top, 1) // You can adjust this padding to control the space above the title
            
            .onAppear {
                //locationManager.startUpdatingLocation()
                //hrbtManager.startScanning()
            }
            
        }
    }
    
    // Fonction pour démarrer le chronomètre
    func startTimer() {
        isTimerActive = true
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { _ in
            timeElapsed += 1
            if timeElapsed % 120 == 0 { // Si 2 minutes se sont écoulées
                intervalNumber += 1 // Passer à l'intervalle suivant
            }
        }
    }    // start test button function
    // Fonction pour arrêter le chronomètre
    func stopTimer() {
        timer?.invalidate()
        isTimerActive = false
    }
    
    func exportCSV() {
        var csvText = "Time, VO2Max, VO2, VCO2, RQ, Speed, Heart\n"
        let dataCount = min(
            bluetoothManager.vo2Maxvalues.count,
            bluetoothManager.vo2values.count,
            bluetoothManager.vco2values.count,
            bluetoothManager.RQvalues.count,
            locationManager.historicalData.count, // Utiliser les données historiques du LocationManager
            hrbtManager.historicalData.count // Pour les données de fréquence cardiaque
        )
        
        for index in 0..<dataCount {
            let time = bluetoothManager.vo2Maxvalues[index].time
            let vo2MaxValue = bluetoothManager.vo2Maxvalues[index].value
            let vo2Value = bluetoothManager.vo2values[index].value
            let vco2Value = bluetoothManager.vco2values[index].value
            let rqValue = bluetoothManager.RQvalues[index].value
            let speed = index < locationManager.historicalData.count ? locationManager.historicalData[index].speed : 0.0
            let heartRate = index < hrbtManager.historicalData.count ? hrbtManager.historicalData[index].heartRate : 0
            
            // Ajouter les données à la ligne CSV
            csvText += "\(time), \(vo2MaxValue), \(vo2Value), \(vco2Value), \(rqValue), \(speed), \(heartRate)\n"
        }
        
        // Convertir en Data
        guard let csvData = csvText.data(using: .utf8) else { return }
        
        
        // Chemin temporaire pour le fichier
        let tempURL = FileManager.default.temporaryDirectory.appendingPathComponent("VO2MaxData_\(Date().formatted()).csv")
        
        // Écrire les données CSV dans le fichier temporaire
        do {
            try csvData.write(to: tempURL)
            showSaveFileDialog(fileURL: tempURL)
        } catch {
            print("Erreur lors de la création du fichier temporaire : \(error.localizedDescription)")
        }
    }
    
    
    
    func showSaveFileDialog(fileURL: URL) {
        let picker = UIDocumentPickerViewController(forExporting: [fileURL], asCopy: true)
        picker.allowsMultipleSelection = false
        
        // Obtenir la scène active et sa fenêtre
        if let scene = UIApplication.shared.connectedScenes.first as? UIWindowScene,
           let rootViewController = scene.windows.first?.rootViewController {
            rootViewController.present(picker, animated: true, completion: nil)
        }
    }

    
}
  extension Date {
      func formatted() -> String {
          let formatter = DateFormatter()
          formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"
          return formatter.string(from: self)
      }
  }

