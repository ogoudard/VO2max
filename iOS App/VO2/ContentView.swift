
import SwiftUI
import Charts
import Foundation
import CoreBluetooth
import UniformTypeIdentifiers
//import BluetoothManager.swift

struct ContentView: View {
    @ObservedObject var bluetoothManager = BluetoothManager()
    private let chartHeight: CGFloat = 200
    @StateObject private var locationManager = LocationManager()
    
    var body: some View {
        NavigationView {
            VStack {
                //Text("VO2Max Tracker")
                //    .font(.largeTitle) // Vous pouvez changer la taille de la police ici
                //    .fontWeight(.bold) // Mettre le texte en gras
                //    .padding(.top, 40) // Ajoute un espacement en haut
                // Bouton "Settings" sous le titre
                HStack {
                    Button(action: {
                        // Action à déclencher quand le bouton est appuyé
                        print("Bouton Settings appuyé")
                    }) {
                        Image(systemName: "gearshape.fill") // Icône de paramètres
                        Text("Settings")
                            .font(.headline)
                    }
                    .padding()
                    Spacer()
                    Button(action: {
                        // Action à déclencher quand le bouton est appuyé
                        print("Bouton Golden Lungs appuyé")
                    }) {
                        Image(systemName: "figure.run") // Icône de paramètres
                        Text("Poumons d'or")
                            .font(.headline)
                    }
                }
                .padding()
                
                .background(Color.blue.opacity(0.1))
                .cornerRadius(8)
                
                
                
                HStack {
                    Button(action:exportCSV) {
                        Image(systemName: "doc.text.fill") // Icône de paramètres
                        Text("Export CSV data file")
                            .font(.headline)
                    }
                }
                .padding()
                
                
                Text("VO2 Max. = \(bluetoothManager.vo2Maxvalue) mL/min/kg")
                    .font(.caption)
                    .padding(.bottom, 2)
                    .onAppear {
                        bluetoothManager.startBluetooth()
                    }
                Text("VO2 Live = \(bluetoothManager.vo2value) mL/min/kg")
                    .font(.caption)
                    .padding(.bottom, 2)
                Text("VCO2 Live = \(bluetoothManager.vco2value) mL/min")
                    .font(.caption)
                    .padding(.bottom, 2)
                Text("RQ Level = \(bluetoothManager.RQvalue) %")
                    .font(.caption)
                    .padding(.bottom, 2)
                Text("O2 = \(bluetoothManager.o2percvalue) %")
                    .font(.caption)
                    .padding(.bottom, 2)
                Text("CO2 = \(bluetoothManager.co2percvalue) %")
                    .font(.caption)
                    .padding(.bottom, 2)
                Text("Vitesse actuelle: \(locationManager.speed*3.6, specifier: "%.2f") km/h")
                    .padding()
                    .font(.title)
                
                     Text("VO2MAX evolution")
                    .font(.caption)
                    .padding(.bottom, 2)
                Chart {
                    ForEach(bluetoothManager.vo2Maxvalues, id: \.time) { entry in
                        LineMark(
                            x: .value("Time", entry.time),
                            y: .value("VO2Max", entry.value)
                        )
                        .foregroundStyle(Color.blue) // Couleur de la courbe
                        .lineStyle(StrokeStyle(lineWidth: 2)) // Épaisseur de la ligne
                        .interpolationMethod(.catmullRom) // Méthode d'interpolation
                    }
                }
                // .padding(.top)
                //.frame(height: 75) // Ajuste la taille du graphique si nécessaire
                Text("VO2 evolution")
                    .font(.caption)
                    .padding(.bottom, 2)
                
                Chart {
                    ForEach(bluetoothManager.vo2values, id: \.time) { entry in
                        LineMark(
                            x: .value("Time", entry.time),
                            y: .value("VO2Max", entry.value)
                        )
                        .foregroundStyle(Color.red) // Couleur de la courbe
                        .lineStyle(StrokeStyle(lineWidth: 2)) // Épaisseur de la ligne
                        .interpolationMethod(.catmullRom) // Méthode d'interpolation
                    }
                }
                
                Text("CO2 evolution")
                    .font(.caption)
                    .padding(.bottom, 2)
                
                //.frame(height: 75) // Ajuste la taille du graphique si nécessaire
                Chart {
                    ForEach(bluetoothManager.vco2values, id: \.time) { entry in
                        LineMark(
                            x: .value("Time", entry.time),
                            y: .value("VO2Max", entry.value)
                        )
                        .foregroundStyle(Color.green) // Couleur de la courbe
                        .lineStyle(StrokeStyle(lineWidth: 2)) // Épaisseur de la ligne
                        .interpolationMethod(.catmullRom) // Méthode d'interpolation
                    }
                }
                
                //.frame(height: 75) // Ajuste la taille du graphique si nécessaire
                
                //.background(Color.white) // Ajoutez une couleur de fond pour mieux visualiser le graphique
                //.border(Color.gray) // Optionnel : ajoutez une bordure pour la visibilité
            }

            .navigationTitle("VO2 MAX") // Titre en haut de la fenêtre
            .navigationBarTitleDisplayMode(.inline) // Centre le titre

            .toolbar {
                ToolbarItem(placement: .principal) {
                    Text("VO2 MAX")
                        .font(.largeTitle) // Définissez la taille ici
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
            .onAppear {
                locationManager.startUpdatingLocation()
            }

            
        }
    }
    
    
    
    func exportCSV() {
        // Créer le contenu du fichier CSV avec les en-têtes de colonne
        var csvText = "Time, VO2Max, VO2, VCO2, RQ\n"
        // Obtenir la longueur minimale des listes pour éviter les erreurs d'index
        let dataCount = min(bluetoothManager.vo2Maxvalues.count,
                            bluetoothManager.vo2values.count,
                            bluetoothManager.vco2values.count,
                            bluetoothManager.RQvalues.count)
        
        // Boucle sur chaque ensemble de données pour écrire les valeurs sur une même ligne
        for index in 0..<dataCount {
            let time = bluetoothManager.vo2Maxvalues[index].time
            let vo2MaxValue = bluetoothManager.vo2Maxvalues[index].value
            let vo2Value = bluetoothManager.vo2values[index].value
            let vco2Value = bluetoothManager.vco2values[index].value
            let rqValue = bluetoothManager.RQvalues[index].value
            
            // Ajouter les données sur une ligne CSV
            csvText += "\(time), \(vo2MaxValue), \(vo2Value), \(vco2Value), \(rqValue)\n"
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
    // Fonction pour ouvrir le dialogue de sauvegarde de fichier
    func showSaveFileDialog(fileURL: URL) {
        let picker = UIDocumentPickerViewController(forExporting: [fileURL], asCopy: true)
        //picker.delegate = context.coordinator // Créez un `Coordinator` si besoin
        picker.allowsMultipleSelection = false
        UIApplication.shared.windows.first?.rootViewController?.present(picker, animated: true, completion: nil)
    }
    
}

  extension Date {
      func formatted() -> String {
          let formatter = DateFormatter()
          formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"
          return formatter.string(from: self)
      }
  }

