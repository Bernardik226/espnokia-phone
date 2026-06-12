class CadeiaDeFontes:
    """Agrega fontes com a mesma interface (jogos()/grupos()): tenta na ordem
    e fica com a primeira resposta com conteúdo; fonte que explodir ou voltar
    vazia passa a vez. Se todas falharem, vale o último resultado bom de
    qualquer uma — alarme e aviso de gol não ficam cegos por soluço de fonte.
    """

    def __init__(self, fontes):
        self.fontes = list(fontes)
        self._jogos = {}
        self._grupos = []

    def _primeira(self, metodo, ultimo):
        for f in self.fontes:
            try:
                out = getattr(f, metodo)()
            except Exception:
                continue
            if out:
                return out
        return ultimo

    def jogos(self):
        self._jogos = self._primeira("jogos", self._jogos)
        return self._jogos

    def grupos(self):
        self._grupos = self._primeira("grupos", self._grupos)
        return self._grupos
