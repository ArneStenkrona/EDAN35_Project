COMMENTS:

Deffered fungerar ju inte med alpha d� man s�tter ett v�rde per pixel

Vi m�ste fixa pipeline s� det �r:

1. Rendera allt f�rutom vatten
2. Bygg enviornment map & shadowmap
3. Bygg scenen med caustics map
4. Rendera scenen igen med vatten d�r man samplar fr�n:
    a) caustics map f�r under vatten caustics
    b) shadowmap f�r shadows
    c) diffuse map f�r alpha blending mellan vatten,
        reflection, och refraction


