#include <BWAPI.h>
#include <BWAPI/Client.h>
#include <BWTA.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <algorithm>

using namespace BWAPI;

double InfluenceGoalAttract(BWAPI::Unit own, BWAPI::Position& pos, BWAPI::Position& goal);
double InfluenceEnemysAttract(BWAPI::Unit own, BWAPI::Position& pos, const BWAPI::Unitset& enemys);
double InfluenceOwnsAttract(BWAPI::Unit own, BWAPI::Position& pos, const BWAPI::Unitset& owns);
double InfluenceEnemysRepel(BWAPI::Unit own, BWAPI::Position& pos, const BWAPI::Unitset& enemys);
double InfluenceOwnsRepel(BWAPI::Unit own, BWAPI::Position& pos, const BWAPI::Unitset& owns);
double InfluenceTerrainRepel(BWAPI::Unit own, BWAPI::Position& pos);

BWAPI::WalkPosition GetAction(BWAPI::Unit own, BWAPI::Position goal);
BWAPI::Unit GetAttackEnemyUnit(BWAPI::Unit own);

bool DrawAttackAction(BWAPI::Unit own, BWAPI::Unit enemy);
bool DrawMoveAction(BWAPI::Unit own, BWAPI::Position& target);
void drawExtendedInterface();

bool flag_draw;
int  lastFrameCount;
int  gameSpeed; 

/** reconnect if connection is broken */
void reconnect()
{
  while(!BWAPIClient.connect())
  {
    std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 });
  }
}


/** main function entry */
int main(int argc, const char* argv[])
{
	std::cout << "Connecting..." << std::endl;;
	reconnect();
	while(true)
	{
		std::cout << "waiting to enter match" << std::endl;
		while ( !Broodwar->isInGame() )
		{
			BWAPI::BWAPIClient.update();
			if (!BWAPI::BWAPIClient.isConnected())
			{
			std::cout << "Reconnecting..." << std::endl;;
			reconnect();
			}
		}
		std::cout << "starting match!" << std::endl;
		Broodwar->sendText("Hello world!");
		Broodwar << "The map is " << Broodwar->mapName() << std::endl;
		// Enable some cheat flags
		Broodwar->enableFlag(Flag::UserInput);
		// Uncomment to enable complete map information
		//Broodwar->enableFlag(Flag::CompleteMapInformation);
		Broodwar->setCommandOptimizationLevel(2); //0--3

		flag_draw = true; 
		lastFrameCount = -1;
		gameSpeed = 42; 
		Broodwar->setLocalSpeed(gameSpeed);

		BWAPI::Position goal(Broodwar->mapWidth() * 32 / 2, Broodwar->mapHeight() * 32 / 2);
		while(Broodwar->isInGame())
		{
			for(auto &e : Broodwar->getEvents())
			{
				BWAPI::Position dest(BWAPI::Broodwar->mapWidth() * 32, BWAPI::Broodwar->mapHeight() / 2 * 32);
				BWAPI::Unit closestEnemy = nullptr;
				switch(e.getType())
				{
				// OnFrame case
				case EventType::MatchFrame:
					drawExtendedInterface();
					if (lastFrameCount >= 0 && Broodwar->getFrameCount() - lastFrameCount < 5)
						continue; 
					// generate, execute, and draw own units actions
					lastFrameCount = Broodwar->getFrameCount();
					for (auto &u : Broodwar->self()->getUnits())
					{
						if (!u->exists())
							continue;
						if (u->isLockedDown() || u->isMaelstrommed() || u->isStasised())
							continue;
						// get highest MOVE/ATTACK action
						BWAPI::WalkPosition wpBestAct = GetAction(u, goal);
						// current position is best, only to attack
						if (wpBestAct.x == 0 && wpBestAct.y == 0)
						{
							// Attack action
							BWAPI::Unit bestEnemy = GetAttackEnemyUnit(u);
							if (bestEnemy)
							{
								u->attack(bestEnemy);
								DrawAttackAction(u, bestEnemy);
							}
						}
						else
						{
							// Move action
							BWAPI::Position pCurrent = u->getPosition();
							BWAPI::Position target(pCurrent.x + wpBestAct.x * 8, pCurrent.y + wpBestAct.y * 8);
							u->rightClick(target);
							DrawMoveAction(u, target);
						}
					}
					break;
				case EventType::MatchEnd:
					if (e.isWinner())
						Broodwar << "I won the game" << std::endl;
					else
						Broodwar << "I lost the game" << std::endl;
					break;
				case EventType::SendText:
					if (e.getText() == "s")
						flag_draw = !flag_draw;
					else if (e.getText() == "+")
					{
						gameSpeed -= 10;
						Broodwar->setLocalSpeed(gameSpeed);
					}
					else if (e.getText() == "-")
					{
						gameSpeed += 10;
						Broodwar->setLocalSpeed(gameSpeed);
					}
					else if (e.getText() == "++")
					{
						gameSpeed = 0;
						Broodwar->setLocalSpeed(gameSpeed);
					}
					else if (e.getText() == "--")
					{
						gameSpeed = 42;
						Broodwar->setLocalSpeed(gameSpeed);
					}
					else
						Broodwar << "You typed \"" << e.getText() << "\"!" << std::endl;
					break;
				case EventType::ReceiveText:
					Broodwar << e.getPlayer()->getName() << " said \"" << e.getText() << "\"" << std::endl;
					break;
				case EventType::PlayerLeft:
					Broodwar << e.getPlayer()->getName() << " left the game." << std::endl;
					break;
				case EventType::NukeDetect:
					break;
				case EventType::UnitCreate:
					break;
				case EventType::UnitDestroy:
					break;
				case EventType::UnitMorph:
					break;
				case EventType::UnitShow:
					break;
				case EventType::UnitHide:
					break;
				case EventType::UnitRenegade:
					break;
				case EventType::SaveGame:
					Broodwar->sendText("The game was saved to \"%s\".", e.getText().c_str());
					break;
				}
			}

			Broodwar->drawTextScreen(300,0,"FPS: %f",Broodwar->getAverageFPS());

			BWAPI::BWAPIClient.update();
			if (!BWAPI::BWAPIClient.isConnected())
			{
			std::cout << "Reconnecting..." << std::endl;
			reconnect();
			}
		}
		std::cout << "Game ended" << std::endl;
	}
	std::cout << "Press ENTER to continue..." << std::endl;
	std::cin.ignore();
	return 0;
}



/** calculate unit ATTRACT influence at certain position from goal 
  * follow the path generated from BWTA::getShortestPath 
  * calculate the average direction of the first 3 TilePosition of path */
double InfluenceGoalAttract(BWAPI::Unit own, BWAPI::Position& pos, BWAPI::Position& goal)
{
	if (!own->exists())
		return 0;
	if (pos.x < 0 || pos.y < 0)
		return 0;
	if (goal.x < 0 || goal.y < 0)
		return 0;

	// use BWTA find shortest path between start and end
	BWAPI::TilePosition tpStart(pos);
	BWAPI::TilePosition tpEnd(goal);
	std::vector<BWAPI::TilePosition> vtpPath = BWTA::getShortestPath(tpStart, tpEnd);

	// accumulate influence of first 3 tileposition on the path to start
	double c = 1;            // initial influence
	double b = 32;           // Gaussian distance bandwidth
	int count = 0;           // only consider first 3 tileposition on path
	double influence = 0;
	for (auto& tp : vtpPath)
	{
		if (tp == tpStart)
			continue;
		if (count >= 3)
			break;
		BWAPI::Position p(tp.x*32+16, tp.y*32+16);
		double d = pos.getDistance(p);
		influence += c*exp(-d*d / b / b);    // Gaussian distance
	}
	return influence;
}

/** calculate ATTRACT influence of unit from enemys
  * attract unit to its own weapon range to enemys
  * related to enemy hitpoint, bonus for damaged enemy 
  * different enemys' influence is maximized */
double InfluenceEnemysAttract(BWAPI::Unit own, BWAPI::Position& pos, const BWAPI::Unitset& enemys)
{
	if (!own->exists() || !own->canAttack())
		return 0;
	if (pos.x < 0 || pos.y < 0)
		return 0;
	if (enemys.size() <= 0) 
		return 0;

	// own unit if can attack Ground/Air
	bool canAttackGround = false;
	bool canAttackAir = false;
	// initial influence
	double cGround = 100;
	double cAir = 100;
	// descend distance slope
	double bGround1;  double bGround2 = 400;
	double bAir1;     double bAir2 = 400;
	// own unit Ground/Air weapon range

	if (own->getType().groundWeapon()) canAttackGround = true;
	if (own->getType().airWeapon())    canAttackAir = true; 
	// TODO: relaxed Ground range for melee weapon
	if (canAttackGround)
	{
		bGround1 = own->getType().groundWeapon().maxRange();
		bGround1 = std::max(1.0, bGround1 - 4);
	}
	if (canAttackAir)
	{
		bAir1 = own->getType().airWeapon().maxRange();
		bAir1 = std::max(1.0, bAir1 - 4);
	}

	double influence = -99999;
	for (auto& eu : enemys)
	{
		if (!eu->exists())
		{
			continue;
		}

		// adjust weight to attack damaged enemy
		double bonus;
		if (eu->getHitPoints() > (0.5*eu->getType().maxHitPoints()))
			bonus = 0;
		else if (eu->getHitPoints() > (0.25*eu->getType().maxHitPoints()))
			bonus = 100;
		else
			bonus = 200;

		double d = pos.getDistance(eu->getPosition());
		d -= (own->getType().dimensionLeft() + own->getType().dimensionRight() + own->getType().dimensionUp() + own->getType().dimensionDown()) / 4; 
		d -= (eu->getType().dimensionLeft() + eu->getType().dimensionRight() + eu->getType().dimensionUp() + eu->getType().dimensionDown()) / 4; 
		d = std::max(1.0, d);
		if (eu->isFlying())
		{
			if (d <= bAir1)
				influence = std::max(influence, bonus + cAir);
			else
				influence = std::max(influence, -(bonus + cAir) / (bAir2 - bAir1)*(d - bAir2)); 
		}
		else
		{
			if (d <= bGround1)
				influence = std::max(influence, bonus + cGround);
			else
				influence = std::max(influence, -(bonus + cGround) / (bGround2 - bGround1)*(d - bGround2)); 
		}
	}
	return influence; 
}

/** calculate unit ATTRACT influence from owns
  * attract follows Gaussian distance 
  * different owns' influence is summed */
double InfluenceOwnsAttract(BWAPI::Unit own, BWAPI::Position& pos, const BWAPI::Unitset& owns)
{
	if (!own->exists())
		return 0;
	if (pos.x < 0 || pos.y < 0)
		return 0;
	if (owns.size() <= 0)
		return 0;

	double k = -0.01;         // descend slope
	double influence = 0;
	for (auto& ou : owns)
	{
		if (!ou->exists())
			continue;
		if (own->getID() == ou->getID())
			continue;

		double d = pos.getDistance(ou->getPosition());
		d -= (own->getType().dimensionRight() + own->getType().dimensionLeft() + own->getType().dimensionUp() + own->getType().dimensionDown()) / 4;
		d -= (ou->getType().dimensionRight() + ou->getType().dimensionLeft() + ou->getType().dimensionUp() + ou->getType().dimensionDown()) / 4; 
		d = std::max(1.0, d);
		influence += k*d;    // linear distance attract with small descend slope
											 // start from (0,0)
	}
	return influence; 
}

/** calculate unit REPEL influence from enemys
  * we don't want own unit be hitted by enemy
  * more easy to retreat if we are damaged
  * different enemys' influence is summed */
double InfluenceEnemysRepel(BWAPI::Unit own, BWAPI::Position& pos, const BWAPI::Unitset& enemys)
{
	if (!own->exists() || !own->canAttack())
		return 0;
	if (pos.x < 0 || pos.y < 0)
		return 0;
	if (enemys.size() <= 0)
		return 0;

	double influence = 0;
	for (auto& eu : enemys)
	{
		if (!eu->exists() || !eu->getType().canAttack())
			continue;

		double d = pos.getDistance(eu->getPosition());
		d -= (own->getType().dimensionRight() + own->getType().dimensionLeft() + own->getType().dimensionUp() + own->getType().dimensionDown()) / 4;
		d -= (eu->getType().dimensionRight() + eu->getType().dimensionLeft() + eu->getType().dimensionUp() + eu->getType().dimensionDown()) / 4;
		d = std::max(1.0, d);

		bool canAttackGround = false;
		bool canAttackAir = false;
		double bGround, bAir; 
		double kGround = 10;		// slope for enemys repel distance
		double kAir = 10;
		if (own->getType().isFlyer() && eu->getType().airWeapon())
		{
			bAir = eu->getType().airWeapon().maxRange();
			bAir += 4; 
			if (d <= bAir)
				influence += kAir*(d - bAir);
			else
				influence += 0;
		}
		else if (eu->getType().groundWeapon())
		{
			bGround = eu->getType().groundWeapon().maxRange();
			bGround += 80;
			if (d <= bGround)
				influence += kGround*(d - bGround);
			else
				influence += 0;
		}
	}

	double weight;
	if (own->getHitPoints() > (0.5*own->getType().maxHitPoints()))
		weight = 1;
	else if (own->getHitPoints() > (0.25*own->getType().maxHitPoints()))
		weight = 2;
	else 
		weight = 4;

	return weight*influence; 
}

/** calculate unit REPEL influence from owns
  * avoid own units be crowded, not good for retreat and move
  * different owns's influence is maximized */
double InfluenceOwnsRepel(BWAPI::Unit own, BWAPI::Position& pos, const BWAPI::Unitset& owns)
{
	if (!own->exists() || own->getType().isFlyer())
		return 0;
	if (pos.x < 0 || pos.y < 0)
		return 0;
	if (owns.size() <= 0)
		return 0;

	double k = 20;			// initial influence
	double b = 8 * 8;		// expected relative distance 
	double influence = 0;
	for (auto& ou : owns)
	{
		if (!ou->exists() || ou->getType().isFlyer())
			continue;
		if (own->getID() == ou->getID())
			continue;

		double d = pos.getDistance(ou->getPosition());
		d -= (own->getType().dimensionRight() + own->getType().dimensionLeft() + own->getType().dimensionUp() + own->getType().dimensionDown()) / 4;
		d -= (ou->getType().dimensionRight() + ou->getType().dimensionLeft() + ou->getType().dimensionUp() + ou->getType().dimensionDown()) / 4;
		d = std::max(1.0, d);
		if (d <= b)
			influence = std::min(influence, k*(d - b)); 
	}
	return influence; 
}

/** calculate unit REPEL influence from terrain obstacles
  * avoid own unit move close to obstacles
  * 4 walkposition is safe enough
  * different obstacles' influence is maximized */
double InfluenceTerrainRepel(BWAPI::Unit own, BWAPI::Position& pos)
{
	if (!own->exists())
		return 0;
	if (pos.x < 0 || pos.y < 0)
		return 0; 

	BWAPI::WalkPosition wpPos(pos);
	if (!Broodwar->isWalkable(wpPos))
		return -1000;

	double c = -1000;
	double influence = 0; 
	for (int step = 1; step <= 20; step++)
	{
		double repel = c - c / 20 * step; 
		for (int i = -step; i <= step; i+=4)
		{
			for (int j = -step; j <= step; j+=4)
			{
				if (std::abs(i) == step || std::abs(j) == step)
				{
					if (!Broodwar->isWalkable(wpPos.x + i, wpPos.y + j))
						influence += repel;
				}
			}
		}
	}
	return influence;
}

/** get next MOVE/ATTACK action 
  * choose unit neighbor walkpositions highest one */
BWAPI::WalkPosition GetAction(BWAPI::Unit own, BWAPI::Position goal)
{
	BWAPI::Position pCurrent = own->getPosition();
	BWAPI::WalkPosition wpBestAct;
	double bestInfluence = -99999;
	for (int i = -6; i <= 6; i+=3)
	{
		for (int j = -6; j <= 6; j+=3)
		{
			BWAPI::WalkPosition wpAct(i, j);
			BWAPI::Position pos(wpAct.x * 8 + pCurrent.x, wpAct.y * 8 + pCurrent.y);
			if (pos.x < 0 || pos.y < 0 || pos.x>Broodwar->mapWidth() * 32 || pos.y>Broodwar->mapHeight() * 32)
				continue;
			double influence = 0;
			// influence += InfluenceGoalAttract(own, pos, goal);
			influence += InfluenceEnemysAttract(own, pos, Broodwar->enemy()->getUnits());
			influence += InfluenceOwnsAttract(own, pos, Broodwar->self()->getUnits());
			influence += InfluenceEnemysRepel(own, pos, Broodwar->enemy()->getUnits());
			influence += InfluenceOwnsRepel(own, pos, Broodwar->self()->getUnits());
			influence += InfluenceTerrainRepel(own, pos);
			if (i == 0 && j == 0)
				influence += 1;
			if (influence > bestInfluence)
			{
				bestInfluence = influence;
				wpBestAct = wpAct;
			}
		}
	}
	return wpBestAct;
}

/** select attack enemy target 
  * now we only consider the lowest hitpoint enemy 
  * TODO: more efficient attack mode */
BWAPI::Unit GetAttackEnemyUnit(BWAPI::Unit own)
{
	// availabel enemys
	BWAPI::Unitset& enemysBeAttackedGround = own->getUnitsInWeaponRange(own->getType().groundWeapon(), BWAPI::Filter::IsEnemy && BWAPI::Filter::Exists);
	BWAPI::Unitset& enemysBeAttackedAir = own->getUnitsInWeaponRange(own->getType().airWeapon(), BWAPI::Filter::IsEnemy && BWAPI::Filter::Exists);
	if (enemysBeAttackedGround.size() <= 0 && enemysBeAttackedAir.size() <= 0)
		return nullptr;

	BWAPI::Unit bestEnemy;
	double minHitPoints = 10000;
	for (auto eu : enemysBeAttackedGround)
	{
		if (minHitPoints > eu->getHitPoints()+eu->getShields()) 
		{
			minHitPoints = eu->getHitPoints() + eu->getShields(); 
			bestEnemy = eu;
		}
	}
	for (auto eu : enemysBeAttackedAir)
	{
		if (minHitPoints > eu->getHitPoints())
		{
			minHitPoints = eu->getHitPoints();
			bestEnemy = eu;
		}
	}
	return bestEnemy;
}

/** draw unit attack action
  * line own unit and its attack target */
bool DrawAttackAction(BWAPI::Unit own, BWAPI::Unit enemy)
{
	if (!flag_draw)
		return false;
	if (!own->exists() || !own->canAttack())
		return false;
	if (!enemy->exists())
		return false;
	if (enemy->isFlying() && !own->getType().airWeapon())
		return false;
	if (!enemy->isFlying() && !own->getType().groundWeapon())
		return false;
	if (!own->getTarget()->exists())
		return false;
	if (own->getTarget()->getID() != enemy->getID())
		return false;
	Broodwar->drawLineMap(own->getPosition(), enemy->getPosition(), BWAPI::Colors::Red);
	return true;
}

/** draw unit move action
  * line unit position and target position*/
bool DrawMoveAction(BWAPI::Unit own, BWAPI::Position& target)
{
	if (!flag_draw)
		return false;
	if (!own->exists() || !own->canMove())
		return false;
	if (!Broodwar->isWalkable(BWAPI::WalkPosition(target)))
		return false;
	Broodwar->drawLineMap(own->getPosition(), target, BWAPI::Colors::White);
	return true;
}

/** draw health bar */
void drawExtendedInterface()
{
	int verticalOffset = -10;

	// draw enemy units
	for (auto & ui : Broodwar->enemy()->getUnits())
	{
		BWAPI::UnitType type(ui->getType());
		int hitPoints = ui->getHitPoints();
		int shields = ui->getShields();

		const BWAPI::Position & pos = ui->getPosition();

		int left = pos.x - type.dimensionLeft();
		int right = pos.x + type.dimensionRight();
		int top = pos.y - type.dimensionUp();
		int bottom = pos.y + type.dimensionDown();

		if (!BWAPI::Broodwar->isVisible(BWAPI::TilePosition(ui->getPosition())))
		{
			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, top), BWAPI::Position(right, bottom), BWAPI::Colors::Grey, false);
			BWAPI::Broodwar->drawTextMap(BWAPI::Position(left + 3, top + 4), "%s", type.getName().c_str());
		}

		if (!type.isResourceContainer() && type.maxHitPoints() > 0)
		{
			double hpRatio = (double)hitPoints / (double)type.maxHitPoints();

			BWAPI::Color hpColor = BWAPI::Colors::Green;
			if (hpRatio < 0.66) hpColor = BWAPI::Colors::Orange;
			if (hpRatio < 0.33) hpColor = BWAPI::Colors::Red;

			int ratioRight = left + (int)((right - left) * hpRatio);
			int hpTop = top + verticalOffset;
			int hpBottom = top + 4 + verticalOffset;

			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), hpColor, true);
			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

			int ticWidth = 3;

			for (int i(left); i < right - 1; i += ticWidth)
			{
				BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
			}
		}

		if (!type.isResourceContainer() && type.maxShields() > 0)
		{
			double shieldRatio = (double)shields / (double)type.maxShields();

			int ratioRight = left + (int)((right - left) * shieldRatio);
			int hpTop = top - 3 + verticalOffset;
			int hpBottom = top + 1 + verticalOffset;

			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), BWAPI::Colors::Blue, true);
			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

			int ticWidth = 3;

			for (int i(left); i < right - 1; i += ticWidth)
			{
				BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
			}
		}

	}

	// draw neutral units and our units
	for (auto & unit : BWAPI::Broodwar->getAllUnits())
	{
		if (unit->getPlayer() == BWAPI::Broodwar->enemy())
		{
			continue;
		}

		const BWAPI::Position & pos = unit->getPosition();

		int left = pos.x - unit->getType().dimensionLeft();
		int right = pos.x + unit->getType().dimensionRight();
		int top = pos.y - unit->getType().dimensionUp();
		int bottom = pos.y + unit->getType().dimensionDown();

		//BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, top), BWAPI::Position(right, bottom), BWAPI::Colors::Grey, false);

		if (!unit->getType().isResourceContainer() && unit->getType().maxHitPoints() > 0)
		{
			double hpRatio = (double)unit->getHitPoints() / (double)unit->getType().maxHitPoints();

			BWAPI::Color hpColor = BWAPI::Colors::Green;
			if (hpRatio < 0.66) hpColor = BWAPI::Colors::Orange;
			if (hpRatio < 0.33) hpColor = BWAPI::Colors::Red;

			int ratioRight = left + (int)((right - left) * hpRatio);
			int hpTop = top + verticalOffset;
			int hpBottom = top + 4 + verticalOffset;

			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), hpColor, true);
			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

			int ticWidth = 3;

			for (int i(left); i < right - 1; i += ticWidth)
			{
				BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
			}
		}

		if (!unit->getType().isResourceContainer() && unit->getType().maxShields() > 0)
		{
			double shieldRatio = (double)unit->getShields() / (double)unit->getType().maxShields();

			int ratioRight = left + (int)((right - left) * shieldRatio);
			int hpTop = top - 3 + verticalOffset;
			int hpBottom = top + 1 + verticalOffset;

			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), BWAPI::Colors::Blue, true);
			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

			int ticWidth = 3;

			for (int i(left); i < right - 1; i += ticWidth)
			{
				BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
			}
		}

		if (unit->getType().isResourceContainer() && unit->getInitialResources() > 0)
		{

			double mineralRatio = (double)unit->getResources() / (double)unit->getInitialResources();

			int ratioRight = left + (int)((right - left) * mineralRatio);
			int hpTop = top + verticalOffset;
			int hpBottom = top + 4 + verticalOffset;

			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), BWAPI::Colors::Cyan, true);
			BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

			int ticWidth = 3;

			for (int i(left); i < right - 1; i += ticWidth)
			{
				BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
			}
		}
	}
}