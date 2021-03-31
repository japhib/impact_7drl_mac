This is my port of Rogue Impact by [Jeff Lait](https://jmlait.itch.io) to mac.

https://jmlait.itch.io/rogue-impact

http://www.zincland.com/7drl/impact

To build & run it, first init submodules:

```
git submodule update --init
```

Then cd into the `./mac` directory and run this:

```
./build.sh && ./impact_7drl
```

---

> Original readme:

Welcome to Rogue Impact!

## ABOUT

Rogue Impact was written in seven days, as part of the Seven Day 
Roguelike Challenge.

    You are an interdimensional traveller who have been jumping between
    worlds with your sibling.  Recently, you became separated, so you have
    more than idle curiosity driving you from world to world.

    In this world you've found a great evil: the demon Baezl'bub
    only the retrieval of his heart from the depths will restore balance
    to this realm.  While you hate to distract from the search for your
    sibling, you know they would expect you to help where you can.

## HOW TO START

If you are a windows user, run it from windows/impact.exe.

If you are a linux user, run it from linux/impact.sh.
You may need to install SDL.

If you are a source diver, go into the src directory.
Look for BUILD.TXT files to try and guide you, there is one under
each of the porting subdirectories.

## HOW TO PLAY

Rogue Impact varies from traditional roguelikes in being party based.
However, like Genshin Impact, only one member of the party is on the
field, so control is very similar to a traditional single player
roguelike, the big difference being you can swap your current active
player.

## CUSTOMIZING 

If you don't like the default font, you can change it!

Go to the windows directory and replace terminal.png with another font
compatible with The Doryen Library.

## GAME DESIGN MESSAGE

If this seems similar to Genshin Impact, it is not an accident.  Which
is not to say Genshin Impact is the original source of these concepts,
just the immediate antecedant I used.

One of the "requirements" of roguelikes is that they are single-player
control.  Usually controlling a party moves you from the genre of
roguelike into the genre of X-com or D&D Gold Box.  However, the
style "Active Character" of Genshin Impact nicely sidesteps this,
allowing the tactics to remain focused on a single avatar despite
the presence of a larger party.  In terms of mechanics, the multiple
party members become an effective health pool - providing the
equivalent amulets of life saving in the face of possibly unfair
deaths.

Elemental reactions are something Genshin Impact claims - at last an
RPG that respects that different forces should interact in a living
world.  This almost is a feature lifted from roguelikes, where the
Nethack genre has the idea of interactions as a core concept.  But I
brought it back into Rogue Impact through an explicit cycle of
elemental vulnerability - being tagged with an element makes a
creature vulnerable to the next in the sequence.  This makes spells
like Cure that are poison based potentially offensive as one could cure
a foe to provide fire vulnerability.

When I pointed out I wanted to make a Genshin Impact inspired
roguelike, a common reaction was to ask if I was going to allow
real money payments...  After all, is it not a gacha game?  But as a
designer, gacha game does not require real life currency, it requires
the random dispenser.  The most obvious component of this in Genshin
Impact is the wishing for characters, hence the idea to use wish
stones you must earn by exploring the dungeon to acquire new party
members.  And those party members?  Randomly selected, leaving you
having to play repeatedly if you want to get the Tier-S Treska that
you read about online.

The other notable mechanic I found in Genshin Impact was how
experience worked.  While technically characters gain experience by
fighting, in practice this does not account for anything.  Instead it
is loot items that grant experience.  The neat effect in a multi-party
game is that you aren't faced with having to feed a new character
until it is relevant - you can instantly dump a bunch of high level
exp items on them and level them quickly.

Unlike Genshin Impact, which is a wonderful example of a smooth on-boarding
in both complexity and difficulty, Rogue Impact has the usual
roguelike wall of difficulty.  You are expected to total-party-kill
many times.

There is a silly mechanic I brought back from Genshin Impact (which
derives from many JRPGs before it).  This is everything has a level.
There is your world level, your dungeon level, your character level,
your weapon level, your ring level...   It is an interesting solution
to the trash-item problem.  But I can't help but think of Everything
is Fodder and what the long-term consequences of this
hyper-commodification are.  

## CREDITS

This is a Seven Day Roguelike.  It was written in a 168 hour time
frame.  Hopefully it doesn't feel like that was the case, however.

Rogue Impact is written by Jeff Lait and uses The Doryen Library
for graphical output.  

You can contact Jeff Lait at: jmlait [snail] gmail [dot] com.

## FINAL IMPORTANT NOTE

Have fun!
