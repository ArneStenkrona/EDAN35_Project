COMMENTS:

Deffered fungerar ju inte med alpha då man sätter ett värde per pixel

Vi måste fixa pipeline så det är:

1. Rendera allt förutom vatten
2. Bygg enviornment map & shadowmap
3. Bygg scenen med caustics map
4. Rendera scenen igen med vatten där man samplar från:
    a) caustics map för under vatten caustics
    b) shadowmap för shadows
    c) diffuse map för alpha blending mellan vatten,
        reflection, och refraction

        
ISSUE: on webGL we can see behind the ball in the refraction.
    This is probably because the rendered image is just rendered
    from the light PoV and then transformed.


