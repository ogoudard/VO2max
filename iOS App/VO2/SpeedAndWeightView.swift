//
//  SpeedAndWeightView.swift
//  VO2
//
//  Created by Goudard Olivier on 22/11/2024.
//

import SwiftUI
import AVFoundation

struct SpeedAndWeightView: View {
    @State private var estimatedSpeed: String = "17.0" // Stocker la vitesse maximale estimée
    @State private var weight: String = "75.0" // Stocker le poids
    @Environment(\.dismiss) var dismiss // Pour revenir à la vue précédente
    @State private var speechSynthesizer = AVSpeechSynthesizer()
    @EnvironmentObject var intervalManager: IntervalManager
    
    func speak(text: String) {
         let utterance = AVSpeechUtterance(string: text)
         utterance.voice = AVSpeechSynthesisVoice(language: "en-US") // Choisissez la langue que vous préférez
         speechSynthesizer.speak(utterance)
     }
   
    // Cacher le clavier
    private func dismissKeyboard() {
        UIApplication.shared.sendAction(#selector(UIResponder.resignFirstResponder), to: nil, from: nil, for: nil)
    }

    var body: some View {
        VStack {
            List(intervalManager.intervals) { interval in
                 HStack {
                     Text("\(interval.time) min")
                     Spacer()
                     Text("\(String(format: "%.1f", interval.speed)) km/h")
                 }
             }
            .padding()
              Text("Enter your max speed estimation and your weight")
                .font(.title2)
                .padding()
            
            // Champ pour la vitesse maximale
            TextField("Max speed (km/h)", text: $estimatedSpeed)
                .keyboardType(.decimalPad) // Permet la saisie de nombres
                .padding()
                .textFieldStyle(RoundedBorderTextFieldStyle())
                .padding(.horizontal)
            
            // Champ pour le poids
            TextField("Weight (kg)", text: $weight)
                .keyboardType(.decimalPad) // Permet la saisie de nombres
                .padding()
                .textFieldStyle(RoundedBorderTextFieldStyle())
                .padding(.horizontal)
            
            // Bouton pour valider les données
            Button("OK") {

                // Action lorsque l'utilisateur valide les entrées
                print("Vitesse estimée : \(estimatedSpeed) km/h")
                print("Poids : \(weight) kg")
                if let maxSpeed = Double(estimatedSpeed) {
                     intervalManager.generateIntervals(maxSpeed: maxSpeed)
                     //dismiss()
                 }
                dismissKeyboard() // Cacher le clavier
                speak(text: "Intervals created. Get ready to suffer!")
                //dismiss() // Revenir à la page principale
            }
            .padding()
            .background(Color.blue)
            .foregroundColor(.white)
            .cornerRadius(8)
            .padding(.top)
            
            // Bouton pour annuler et revenir
            Button("Start Test") {
                dismiss() // Revenir sans enregistrer
            }
            .padding()
            .background(Color.blue)
            .foregroundColor(.white)
            .cornerRadius(8)
            .padding(.top)
            
        
        }
        .padding()
        .navigationTitle("Test detail")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear {
            // Générer les intervalles par défaut pour 20 km/h
            if let defaultSpeed = Double(estimatedSpeed) {
                intervalManager.generateIntervals(maxSpeed: defaultSpeed)
            }
        }
    }
}

struct Interval: Identifiable {
    let id = UUID()
    let time: Int // Temps en minutes
    let speed: Double // Vitesse en km/h
}


class IntervalManager: ObservableObject {
    @Published var intervals: [Interval] = []

    func generateIntervals(maxSpeed: Double) {
        let startingSpeed = maxSpeed - 6
        let stepCount = 6
        let stepSize = (maxSpeed - startingSpeed) / Double(stepCount)
        var newIntervals: [Interval] = []

        // Générer les intervalles progressifs jusqu'à la vitesse max estimée
        for i in 0...stepCount {
            let speed = startingSpeed + stepSize * Double(i)
            newIntervals.append(Interval(time: i * 2, speed: speed))
        }

        // Ajouter trois intervalles au-delà de la vitesse max estimée
        for i in 1...3 {
            let speed = maxSpeed + Double(i)
            newIntervals.append(Interval(time: (stepCount + i) * 2, speed: speed))
        }

        intervals = newIntervals
    }
}
