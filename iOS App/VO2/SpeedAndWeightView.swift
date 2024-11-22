//
//  SpeedAndWeightView.swift
//  VO2
//
//  Created by Goudard Olivier on 22/11/2024.
//

import SwiftUI
import AVFoundation

struct SpeedAndWeightView: View {
    @State private var estimatedSpeed: String = "" // Stocker la vitesse maximale estimée
    @State private var weight: String = "" // Stocker le poids
    @Environment(\.dismiss) var dismiss // Pour revenir à la vue précédente
    @State private var speechSynthesizer = AVSpeechSynthesizer()
    
    func speak(text: String) {
         let utterance = AVSpeechUtterance(string: text)
         utterance.voice = AVSpeechSynthesisVoice(language: "en-US") // Choisissez la langue que vous préférez
         speechSynthesizer.speak(utterance)
     }

    var body: some View {
        VStack {
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
                speak(text: "Intervals created. Get ready to suffer!")
                // Vous pouvez ici enregistrer les valeurs ou les utiliser dans votre logique
                dismiss() // Revenir à la page principale
            }
            .padding()
            .background(Color.green)
            .foregroundColor(.white)
            .cornerRadius(8)
            .padding(.top)
            
            // Bouton pour annuler et revenir
            Button("Dismmis") {
                dismiss() // Revenir sans enregistrer
            }
            .padding()
            .background(Color.red)
            .foregroundColor(.white)
            .cornerRadius(8)
            .padding(.top)
        }
        .padding()
        .navigationTitle("Détails du Test")
        .navigationBarTitleDisplayMode(.inline)
    }
    

}
