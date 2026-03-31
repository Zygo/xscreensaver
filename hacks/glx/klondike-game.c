/* klondike, Copyright (c) 2024  Joshua Timmons <josh@developerx.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#include "xlockmore.h"
#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "gltrackball.h"
#include "klondike-game.h"

// static const char *suits[] = {"Diamonds", "Clubs", "Hearts", "Spades"};
// static const char *short_suits[] = {"D", "C", "H", "S"};
// static const char *ranks[] = {"", "Ace", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten", "Jack", "Queen", "King"};
// static const char *short_ranks[] = {"", "A", "2", "3", "4", "5", "6", "7", "8", "9", "T", "J", "Q", "K"};

static void remove_card_from_deck(game_state_struct *, card_struct *);

// initialize the deck
void klondike_initialize_deck(klondike_configuration *bp)
{
    card_struct *deck = bp->game_state->deck;
    int index = 0;

    for (int suit = 0; suit < NUM_SUITS; suit++)
    {
        for (int rank = ACE; rank <= KING; rank++)
        {

            deck[index].suit = (Suit)suit;
            deck[index].rank = (Rank)rank;
            deck[index].is_face_up = 0;
            deck[index].x = bp->deck_x;
            deck[index].y = bp->deck_y;
            deck[index].start_x = bp->deck_x + RANDOM_POSITION_OFFSET;
            deck[index].start_y = bp->deck_y + RANDOM_POSITION_OFFSET;
            deck[index].dest_x = bp->deck_x + RANDOM_POSITION_OFFSET;
            deck[index].dest_y = bp->deck_y + RANDOM_POSITION_OFFSET;
            deck[index].start_frame = 0;
            deck[index].end_frame = 0;
            deck[index].start_angle = 0.0f;
            deck[index].end_angle = 0.0f;
            deck[index].angle = 0.0f;
            deck[index].z = 0.0f;
            deck[index].start_z = 0.0f;
            index++;
        }
    }
}

// shuffle the deck
void klondike_shuffle_deck(card_struct deck[])
{
    for (int i = NUM_CARDS - 1; i > 0; i--)
    {
        int j = random() % (i + 1);
        card_struct temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

// deal the cards to the tableaus
void klondike_deal_cards(klondike_configuration *bp)
{
    game_state_struct *game_state = bp->game_state;

    for (int i = 0; i < 7; i++)
    {
        game_state->tableau_size[i] = 0;

        for (int j = 0; j <= i; j++)
        {
            game_state->tableau[i][j] = game_state->deck[0];
            remove_card_from_deck(game_state, &game_state->tableau[i][j]);
            if (j == i)
            {
                game_state->tableau[i][j].is_face_up = 1; // The top card of each pile is face up
            }
            game_state->tableau_size[i]++;
        }
    }

    game_state->waste_size = 0;
    game_state->foundation_size[CLUBS] = 0;
    game_state->foundation_size[DIAMONDS] = 0;
    game_state->foundation_size[HEARTS] = 0;
    game_state->foundation_size[SPADES] = 0;
    game_state->moves = 0;
    game_state->moves_since_waste_flip = 0;
}

// clone the game state
static game_state_struct *clone_game_state(game_state_struct *game_state)
{
    game_state_struct *newgame_state = (game_state_struct *)malloc(sizeof(game_state_struct));
    for (int i = 0; i < NUM_CARDS; i++)
    {
        newgame_state->deck[i] = game_state->deck[i];
    }
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 20; j++)
        {
            newgame_state->tableau[i][j] = game_state->tableau[i][j];
        }
        newgame_state->tableau_size[i] = game_state->tableau_size[i];
    }
    for (int i = 0; i < MAX_WASTE; i++)
    {
        newgame_state->waste[i] = game_state->waste[i];
    }
    newgame_state->waste_size = game_state->waste_size;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < MAX_FOUNDATION; j++)
        {
            newgame_state->foundation[i][j] = game_state->foundation[i][j];
        }
        newgame_state->foundation_size[i] = game_state->foundation_size[i];
    }

    newgame_state->moves = game_state->moves;
    newgame_state->moves_since_waste_flip = game_state->moves_since_waste_flip;

    return newgame_state;
}

// free the game state
void klondike_free_game_state(game_state_struct *game_state)
{
    free(game_state);
}

// get the number of cards remaining in the deck by subtracting the sum of the tableau, foundations, and waste size from 52
int klondike_deck_size(game_state_struct *game_state)
{
    // count the cards in the tableau
    int tableauSize = 0;
    for (int i = 0; i < 7; i++)
    {
        tableauSize += game_state->tableau_size[i];
    }

    // count the cards in the foundation
    int foundation_size = 0;
    for (int i = 0; i < 4; i++)
    {
        foundation_size += game_state->foundation_size[i];
    }

    // count the cards in the waste
    int waste_size = game_state->waste_size;
    int ret = 52 - tableauSize - foundation_size - waste_size;

    return ret;
}

// reset the waste pile
static void reset_waste(klondike_configuration *bp, game_state_struct *game_state)
{
    for (int i = 0; i < game_state->waste_size; i++)
    {
        game_state->deck[i] = game_state->waste[i];

        card_struct *animated_card = &game_state->deck[i];
        animated_card->start_frame = bp->tick + (i+5) * bp->animation_ticks / 3;
        animated_card->end_frame = bp->tick + (i+5) * bp->animation_ticks / 3 + bp->animation_ticks;
        animated_card->start_x = animated_card->x;
        animated_card->start_y = animated_card->y;
        animated_card->dest_x = bp->deck_x + RANDOM_POSITION_OFFSET;
        animated_card->dest_y = bp->deck_y + RANDOM_POSITION_OFFSET;
        animated_card->start_angle = 180.0f;
        animated_card->end_angle = 360.0f;
        animated_card->start_z = animated_card->z;
        
        game_state->waste[i].rank = NONE;
        game_state->waste[i].suit = 0;
        game_state->waste[i].is_face_up = 0;
    }

    game_state->waste_size = 0;
    game_state->moves_since_waste_flip = 0;
}

// remove a card from the deck and move the subsequent cards up
static void remove_card_from_deck(game_state_struct *state, card_struct *card)
{
    for (int i = 0; i < NUM_CARDS; i++)
    {
        if (state->deck[i].suit == card->suit && state->deck[i].rank == card->rank)
        {
            for (int j = i; j < NUM_CARDS - 1; j++)
            {
                state->deck[j] = state->deck[j + 1];
            }

            state->deck[NUM_CARDS - 1].rank = 0;
            state->deck[NUM_CARDS - 1].suit = 0;
            state->deck[NUM_CARDS - 1].is_face_up = 0;
            break;
        }
    }
}

// find the card in either the tableau or the waste pile
// and move it to the foundation
static game_state_struct *move_card_to_foundation(klondike_configuration *bp, game_state_struct *game_state, card_struct *card)
{
    //  create a new game_state clone
    game_state_struct *ret = clone_game_state(game_state);

    for (int i = 0; i < 7; i++)
    {
        int j = ret->tableau_size[i] - 1;
        if (ret->tableau[i][j].rank == (card->rank) && ret->tableau[i][j].suit == card->suit && card->is_face_up)
        {
            if (card->rank == ACE || ret->foundation[card->suit][ret->foundation_size[card->suit] - 1].rank == card->rank - 1)
            {
                ret->foundation[card->suit][ret->foundation_size[card->suit]] = game_state->tableau[i][j];
                ret->tableau_size[i]--;
                for (int k = j; k < ret->tableau_size[i]; k++)
                {
                    ret->tableau[i][k] = ret->tableau[i][k + 1];
                }

                ret->foundation[card->suit][ret->foundation_size[card->suit]].start_frame = bp->tick;
                ret->foundation[card->suit][ret->foundation_size[card->suit]].end_frame = bp->tick + bp->animation_ticks;
                ret->foundation[card->suit][ret->foundation_size[card->suit]].start_x = ret->foundation[card->suit][ret->foundation_size[card->suit]].x;
                ret->foundation[card->suit][ret->foundation_size[card->suit]].start_y = ret->foundation[card->suit][ret->foundation_size[card->suit]].y;
                ret->foundation[card->suit][ret->foundation_size[card->suit]].dest_x = bp->foundation_placeholders[card->suit].x;
                ret->foundation[card->suit][ret->foundation_size[card->suit]].dest_y = bp->foundation_placeholders[card->suit].y;
                ret->foundation[card->suit][ret->foundation_size[card->suit]].start_angle = ret->foundation[card->suit][ret->foundation_size[card->suit]].end_angle;
                ret->foundation[card->suit][ret->foundation_size[card->suit]].start_z = ret->foundation[card->suit][ret->foundation_size[card->suit]].z + 3;

                ret->foundation_size[card->suit]++;

                //("Moved %s of %s from tableau to foundation\n", ranks[card->rank], suits[card->suit]);
                return ret;
            }
        }
    }

    if (ret->waste[ret->waste_size - 1].rank == card->rank && ret->waste[ret->waste_size - 1].suit == card->suit && card->is_face_up)
    {
        if (card->rank == ACE || ret->foundation[card->suit][ret->foundation_size[card->suit] - 1].rank == card->rank - 1)
        {
            ret->foundation[card->suit][ret->foundation_size[card->suit]] = game_state->waste[ret->waste_size - 1];

            ret->foundation[card->suit][ret->foundation_size[card->suit]].start_frame = bp->tick;
            ret->foundation[card->suit][ret->foundation_size[card->suit]].end_frame = bp->tick + bp->animation_ticks;
            ret->foundation[card->suit][ret->foundation_size[card->suit]].start_x = ret->foundation[card->suit][ret->foundation_size[card->suit]].x;
            ret->foundation[card->suit][ret->foundation_size[card->suit]].start_y = ret->foundation[card->suit][ret->foundation_size[card->suit]].y;
            ret->foundation[card->suit][ret->foundation_size[card->suit]].dest_x = bp->foundation_placeholders[card->suit].x;
            ret->foundation[card->suit][ret->foundation_size[card->suit]].dest_y = bp->foundation_placeholders[card->suit].y;
            ret->foundation[card->suit][ret->foundation_size[card->suit]].start_angle = ret->foundation[card->suit][ret->foundation_size[card->suit]].end_angle;
            ret->foundation[card->suit][ret->foundation_size[card->suit]].start_z = ret->foundation[card->suit][ret->foundation_size[card->suit]].z + 3;

            ret->foundation_size[card->suit]++;
            ret->waste_size--;

            return ret;
        }
    }

    klondike_free_game_state(ret);
    return NULL;
}

// Move the king with the fewest hidden cards on tableau pile to empty tableau
static game_state_struct *move_king_to_empty_tableau(klondike_configuration *bp)
{
    game_state_struct *ret = clone_game_state(bp->game_state);
    int minHidden = 20;
    int minPile = -1;
    for (int i = 0; i < 7; i++)
    {
        if (ret->tableau_size[i] > 0 && ret->tableau[i][ret->tableau_size[i] - 1].rank == KING)
        {
            int hidden = 0;
            for (int j = 0; j < ret->tableau_size[i] - 1; j++)
            {
                if (!ret->tableau[i][j].is_face_up)
                {
                    hidden++;
                }
            }
            if (hidden < minHidden && hidden > 0)
            {
                minHidden = hidden;
                minPile = i;
            }
        }
    }
    if (minPile != -1)
    {
        for (int i = 0; i < 7; i++)
        {
            if (ret->tableau_size[i] == 0)
            {
                ret->tableau[i][0] = ret->tableau[minPile][ret->tableau_size[minPile] - 1];
                ret->tableau_size[i]++;
                ret->tableau_size[minPile]--;

                card_struct *animated_card = &ret->tableau[i][0];
                animated_card->start_frame = bp->tick;
                animated_card->end_frame = bp->tick + bp->animation_ticks;
                animated_card->start_x = animated_card->x;
                animated_card->start_y = animated_card->y;
                animated_card->dest_x = bp->tableau_placeholders[i].x + RANDOM_POSITION_OFFSET;
                animated_card->dest_y = bp->tableau_placeholders[i].y + RANDOM_POSITION_OFFSET;
                animated_card->start_angle = 180.0f;
                animated_card->end_angle = 180.0f;
                animated_card->start_z = animated_card->z;

                return ret;
            }
        }
    }

    klondike_free_game_state(ret);
    return NULL;
}

// Allow moves to a tableau if the top card on the destination tableau is the opposite color and one rank higher
static int can_move_to_tableau(game_state_struct *game_state, card_struct *card, int toPile)
{
    // which pile is the card in?
    int pile = -1;
    for (int i = 0; i < 7 && pile == -1; i++)
    {
        for (int j = 0; j < game_state->tableau_size[i]; j++)
        {
            if (game_state->tableau[i][j].rank == card->rank && game_state->tableau[i][j].suit == card->suit)
            {
                pile = i;
                break;
            }
        }
    }

    // number of face down cards in that pile
    int hidden = 0;
    if (pile != -1)
    {
        for (int i = 0; i < game_state->tableau_size[pile] - 1; i++)
        {
            if (!game_state->tableau[pile][i].is_face_up)
            {
                hidden++;
            }
        }
    }

    if (hidden > 0 && game_state->tableau_size[toPile] == 0 && card->rank == KING && card->is_face_up)
    {
        return 1;
    }
    card_struct *topCard = &game_state->tableau[toPile][game_state->tableau_size[toPile] - 1];
    if (card->is_face_up && topCard->is_face_up && card->rank == topCard->rank - 1 && card->suit % 2 != topCard->suit % 2)
    {
        return 1;
    }

    return 0;
}

// TODO: Prefer moving tableaus with more hidden cards
// Move the visible cards from one table tableau onto another tableau if the top card of the destination tableau is the 
//opposite color and one rank higher than the bottom card of the source tableau
static game_state_struct *move_tableau_base_card_to_tableau(klondike_configuration *bp)
{
    game_state_struct *game_state = bp->game_state;
    game_state_struct *ret = clone_game_state(game_state);

    for (unsigned int preferredRank = 13; preferredRank > 0; preferredRank--)
    {
        for (int preferredHidden = 6; preferredHidden >= 0; preferredHidden--)
        {
            for (int i = 0; i < 7; i++)
            {
                // i is the index of the source tableau
                // proceed if the number of hidden cards in tableau[i] is equal to preferredHidden
                if (ret->tableau_size[i] >= 0)
                {
                    int hidden = 0;
                    for (int j = 0; j < ret->tableau_size[i] - 1; j++)
                    {
                        if (!ret->tableau[i][j].is_face_up)
                        {
                            hidden++;
                        }
                    }

                    if (hidden != preferredHidden)
                    {
                        continue;
                    }

                    for (int j = 0; j < 7; j++)
                    {
                        if (i != j && ret->tableau_size[j] >= 0)
                        {
                            // base case is the first face up card in the tableau
                            int baseCardIndex = -1;
                            for (int k = 0; k < ret->tableau_size[i]; k++)
                            {
                                if (ret->tableau[i][k].is_face_up)
                                {
                                    baseCardIndex = k;
                                    break;
                                }
                            }

                            if (baseCardIndex > -1 && ret->tableau[i][baseCardIndex].rank == preferredRank && can_move_to_tableau(ret, &ret->tableau[i][baseCardIndex], j))
                            {
                                int is_face_up = 0;
                                for (int l = 0; l < ret->tableau_size[j]; l++)
                                {
                                    if (ret->tableau[j][l].is_face_up)
                                    {
                                        is_face_up++;
                                    }
                                }

                                for (int k = baseCardIndex; k < ret->tableau_size[i]; k++)
                                {
                                    ret->tableau[j][ret->tableau_size[j]] = ret->tableau[i][k];

                                    card_struct *animated_card = &ret->tableau[j][ret->tableau_size[j]];
                                    animated_card->start_frame = bp->tick;
                                    animated_card->end_frame = bp->tick + bp->animation_ticks;
                                    animated_card->start_x = animated_card->x;
                                    animated_card->start_y = animated_card->y;
                                    animated_card->dest_x = bp->tableau_placeholders[j].x + RANDOM_POSITION_OFFSET;
                                    animated_card->dest_y = bp->tableau_placeholders[j].y - (is_face_up + k - baseCardIndex) * 0.05 + RANDOM_POSITION_OFFSET;
                                    animated_card->start_angle = animated_card->end_angle;
                                    animated_card->start_z = animated_card->z;

                                    ret->tableau_size[j]++;
                                }
                                ret->tableau_size[i] = baseCardIndex;

                                return ret;
                            }
                        }
                    }
                }
            }
        }
    }

    klondike_free_game_state(ret);
    return NULL;
}

// find the next card for each foundation and see if it is one of the visible cards on one of the tableaus
static game_state_struct *reveal_foundation_move(klondike_configuration *bp)
{
    game_state_struct *ret = clone_game_state(bp->game_state);
    for (int i = 0; i < 4; i++)
    {
        if (ret->foundation_size[i] > 0)
        {
            for (int j = 0; j < 7; j++)
            {
                for (int k = 0; k < ret->tableau_size[j]; k++)
                {
                    if (ret->tableau[j][k].rank == ret->foundation[i][ret->foundation_size[i] - 1].rank + 1 && ret->tableau[j][k].is_face_up == 1 && ret->foundation[i][ret->foundation_size[i] - 1].suit == ret->tableau[j][k].suit)
                    {
                        // see if the the card at ret->tableau[j][k+1] can be moved to another foundation
                        if (ret->tableau_size[j] > k + 1)
                        {
                            game_state_struct *ret2 = NULL;
                            if ((ret2 = move_card_to_foundation(bp, ret, &ret->tableau[j][k + 1])))
                            {
                                klondike_free_game_state(ret);
                                return ret2;
                            }
                        }
                    }
                }
            }
        }
    }

    klondike_free_game_state(ret);
    return NULL;
}

// Move the base card of a tableau to the foundation if possible
static game_state_struct *move_tableau_base_card_to_foundation(klondike_configuration *bp)
{
    game_state_struct *ret = clone_game_state(bp->game_state);
    for (int i = 0; i < 7; i++)
    {
        if (ret->tableau_size[i] > 0)
        {
            for (int j = 0; j < 4; j++)
            {
                if (ret->foundation_size[j] == 0 && ret->tableau[i][ret->tableau_size[i] - 1].rank == ACE)
                {
                    game_state_struct *ret2;

                    if ((ret2 = move_card_to_foundation(bp, ret, &ret->tableau[i][ret->tableau_size[i] - 1])))
                    {
                        klondike_free_game_state(ret);
                        return ret2;
                    }
                }
                else if (ret->foundation_size[j] > 0 && ret->foundation[j][ret->foundation_size[j] - 1].rank == ret->tableau[i][ret->tableau_size[i] - 1].rank - 1 && ret->foundation[j][ret->foundation_size[j] - 1].suit == ret->tableau[i][ret->tableau_size[i] - 1].suit && ret->tableau[i][ret->tableau_size[i] - 1].is_face_up)
                {
                    game_state_struct *ret2;
                    if ((ret2 = move_card_to_foundation(bp, ret, &ret->tableau[i][ret->tableau_size[i] - 1])))
                    {
                        klondike_free_game_state(ret);
                        return ret2;
                    }
                }
            }
        }
    }

    klondike_free_game_state(ret);
    return NULL;
}

// Move the top card on the waste pile to one of the foundations if possible
static game_state_struct *move_waste_to_foundation(klondike_configuration *bp)
{
    game_state_struct *ret = clone_game_state(bp->game_state);
    if (ret->waste_size > 0)
    {
        for (unsigned int i = 0; i < 4; i++)
        {
            if ((ret->foundation_size[i] > 0 && ret->foundation[i][ret->foundation_size[i] - 1].rank == ret->waste[ret->waste_size - 1].rank - 1) || (ret->foundation_size[i] == 0 && ret->waste[ret->waste_size - 1].rank == ACE && ret->waste[ret->waste_size - 1].suit == i))
            {
                game_state_struct *ret2;
                if ((ret2 = move_card_to_foundation(bp, ret, &ret->waste[ret->waste_size - 1])))
                {
                    klondike_free_game_state(ret);
                    ret = NULL;
                    return ret2;
                }
            }
        }
    }

    klondike_free_game_state(ret);
    return NULL;
}

// move the top card on the waste pile to one of the tableaus if possible
static game_state_struct *move_waste_to_tableau(klondike_configuration *bp)
{
    game_state_struct *ret = clone_game_state(bp->game_state);
    if (ret->waste_size > 0)
    {
        for (int i = 0; i < 7; i++)
        {
            if (ret->tableau_size[i] > 0 && ret->tableau[i][ret->tableau_size[i] - 1].rank == ret->waste[ret->waste_size - 1].rank + 1 && ret->tableau[i][ret->tableau_size[i] - 1].suit % 2 != ret->waste[ret->waste_size - 1].suit % 2)
            {
                ret->tableau[i][ret->tableau_size[i]] = ret->waste[ret->waste_size - 1];

                int is_face_up = 0;
                for (int k = 0; k < ret->tableau_size[i]; k++)
                {
                    if (ret->tableau[i][k].is_face_up)
                    {
                        is_face_up++;
                    }
                }

                card_struct *animated_card = &ret->tableau[i][ret->tableau_size[i]];
                animated_card->start_frame = bp->tick;
                animated_card->end_frame = bp->tick + bp->animation_ticks;
                animated_card->start_x = animated_card->x;
                animated_card->start_y = animated_card->y;
                animated_card->dest_x = bp->tableau_placeholders[i].x + RANDOM_POSITION_OFFSET;
                animated_card->dest_y = bp->tableau_placeholders[i].y - (is_face_up) * 0.05 + RANDOM_POSITION_OFFSET;
                animated_card->start_angle = 180.0f;
                animated_card->end_angle = 180.0f;
                animated_card->start_z = animated_card->z;

                ret->tableau_size[i]++;
                ret->waste_size--;
                return ret;
            }

            if (ret->tableau_size[i] == 0 && ret->waste[ret->waste_size - 1].rank == KING)
            {
                ret->tableau[i][0] = ret->waste[ret->waste_size - 1];

                int is_face_up = 0;

                card_struct *animated_card = &ret->tableau[i][0];
                animated_card->start_frame = bp->tick;
                animated_card->end_frame = bp->tick + bp->animation_ticks;
                animated_card->start_x = animated_card->x;
                animated_card->start_y = animated_card->y;
                animated_card->dest_x = bp->tableau_placeholders[i].x + RANDOM_POSITION_OFFSET;
                animated_card->dest_y = bp->tableau_placeholders[i].y - (is_face_up) * 0.05 + RANDOM_POSITION_OFFSET;
                animated_card->start_angle = 180.0f;
                animated_card->end_angle = 180.0f;
                animated_card->start_z = animated_card->z;

                ret->tableau_size[i]++;
                ret->waste_size--;
                return ret;
            }
        }
    }

    klondike_free_game_state(ret);
    return NULL;
}

// move a card from the deck to the waste pile if there is at least one card remaining in the deck
static game_state_struct *move_deck_to_waste(klondike_configuration *bp)
{
    game_state_struct *ret = clone_game_state(bp->game_state);

    // get the number of cards already on the board
    int boardSize = 0;
    for (int i = 0; i < 7; i++)
    {
        boardSize += ret->tableau_size[i];
    }
    for (int i = 0; i < 4; i++)
    {
        boardSize += ret->foundation_size[i];
    }
    boardSize += ret->waste_size;

    if (boardSize == 52)
    {
        klondike_free_game_state(ret);
        return NULL;
    }

    for (int i = 0; i < bp->draw_count; i++)
    {
        if (boardSize < 52)
        {
            ret->waste[ret->waste_size] = ret->deck[0];
            ret->waste[ret->waste_size].is_face_up = 1;

            card_struct *animated_card = &ret->waste[ret->waste_size];
            animated_card->start_frame = bp->tick + bp->animation_ticks / 4 * i;
            animated_card->end_frame = animated_card->start_frame + bp->animation_ticks;
            animated_card->start_x = bp->deck_x;
            animated_card->start_y = bp->deck_y;
            animated_card->dest_x = bp->waste_x + 0.025 * ret->waste_size + RANDOM_POSITION_OFFSET;
            animated_card->dest_y = bp->waste_y + RANDOM_POSITION_OFFSET;
            animated_card->start_angle = 0.0f;
            animated_card->end_angle = 180.0f;
            animated_card->start_z = animated_card->z;

            ret->waste_size++;

            remove_card_from_deck(ret, &ret->deck[0]);
        }

        boardSize++;
    }

    return ret;
}

// Turn any over last tableau card that is face down
static game_state_struct *turn_over_last_tableau_card(klondike_configuration *bp)
{
    game_state_struct *ret = clone_game_state(bp->game_state);
    for (int i = 0; i < 7; i++)
    {
        if (ret->tableau_size[i] > 0 && ret->tableau[i][ret->tableau_size[i] - 1].is_face_up == 0)
        {
            ret->tableau[i][ret->tableau_size[i] - 1].is_face_up = 1;

            ret->tableau[i][ret->tableau_size[i] - 1].start_frame = bp->tick;
            ret->tableau[i][ret->tableau_size[i] - 1].end_frame = bp->tick + bp->animation_ticks;
            ret->tableau[i][ret->tableau_size[i] - 1].start_x = ret->tableau[i][ret->tableau_size[i] - 1].x;
            ret->tableau[i][ret->tableau_size[i] - 1].start_y = ret->tableau[i][ret->tableau_size[i] - 1].y;
            ret->tableau[i][ret->tableau_size[i] - 1].dest_x = ret->tableau[i][ret->tableau_size[i] - 1].x;
            ret->tableau[i][ret->tableau_size[i] - 1].dest_y = ret->tableau[i][ret->tableau_size[i] - 1].y;
            ret->tableau[i][ret->tableau_size[i] - 1].start_angle = 0.0f;
            ret->tableau[i][ret->tableau_size[i] - 1].end_angle = 180.0f;

            return ret;
        }
    }

    klondike_free_game_state(ret);
    return NULL;
}

// This function should return a new game state that is the result of making the next move in the game
// The function should not modify the original game state
// The function should return NULL if there are no possible moves
static game_state_struct *next_move_inner(klondike_configuration *bp)
{
    game_state_struct *ret = NULL;

    if ((ret = turn_over_last_tableau_card(bp)))
    {
        ret->moves_since_waste_flip++;
        return ret;
    }

    if ((ret = move_tableau_base_card_to_foundation(bp)))
    {
        ret->moves_since_waste_flip++;
        return ret;
    }

    if ((ret = move_king_to_empty_tableau(bp)))
    {
        ret->moves_since_waste_flip++;
        return ret;
    }

    if ((ret = move_tableau_base_card_to_tableau(bp)))
    {
        ret->moves_since_waste_flip++;
        return ret;
    }

    if ((ret = reveal_foundation_move(bp)))
    {
        ret->moves_since_waste_flip++;
        return ret;
    }

    if ((ret = move_waste_to_foundation(bp)))
    {
        ret->moves_since_waste_flip++;
        return ret;
    }

    if ((ret = move_waste_to_tableau(bp)))
    {
        ret->moves_since_waste_flip++;
        return ret;
    }

    if ((ret = move_deck_to_waste(bp)))
    {
        return ret;
    }

    if (bp->game_state->moves_since_waste_flip > 0)
    {
        ret = clone_game_state(bp->game_state);
        reset_waste(bp, ret);
        return ret;
    }

    return NULL;
}

game_state_struct *klondike_next_move(klondike_configuration *bp)
{
    game_state_struct *ret = NULL;
    if ((ret = next_move_inner(bp)))
    {
        ret->moves++;

        // zero the cards in the foundations arrays past the foundation length
        for (int i = 0; i < 4; i++)
        {
            for (int j = ret->foundation_size[i]; j < 13; j++)
            {
                ret->foundation[i][j].rank = NONE;
                ret->foundation[i][j].suit = 0;
                ret->foundation[i][j].is_face_up = 0;
            }
        }

        // zero the cards in the tableau arrays past the tableau length
        for (int i = 0; i < 7; i++)
        {
            for (int j = ret->tableau_size[i]; j < 13; j++)
            {
                ret->tableau[i][j].rank = 0;
                ret->tableau[i][j].suit = 0;
                ret->tableau[i][j].is_face_up = 0;
            }
        }

        // zero the cards in the waste array past the waste length
        for (int i = ret->waste_size; i < 24; i++)
        {
            ret->waste[i].rank = 0;
            ret->waste[i].suit = 0;
            ret->waste[i].is_face_up = 0;
        }

        return ret;
    }

    return NULL;
}
