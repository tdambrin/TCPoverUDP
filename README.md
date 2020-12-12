*client 1:
    - Au premier segment reçu : "I need something bigger"
        => fenêtre normal et suppression de l'envoie de la taille du fichier côté server

    - Rcv a duplicate ACK puis freeze
        => start au numSeq 0 (et pas 10)

    (i) Première idée de comportement : renvoie les ACK dans l'ordre inverse/aléatoire + des timeout

*Problème :
    - vérifier le cas ou la retransmission dupack echoue et qu'on la renvoie jamais -> compteur de dupAck ignoré

*NEW : 
    - commencer avec une fenetre = 5% de lenSeqN et idem dans timeout
    - essayé de réduire le nombre de timeout envoyé -> fonctionne pas (recherche pas très poussée)
    - incrémentation de la fenêtre à chaque ACK recu et pas à seulement aux ACK continus. 