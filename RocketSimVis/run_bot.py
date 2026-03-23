import rlgym_sim
from rlgym_sim.utils.obs_builders import AdvancedObs
from rlgym_sim.utils.action_parsers import DefaultAction
from rlgym_sim.utils.terminal_conditions.common_conditions import TimeoutCondition, GoalScoredCondition
from rlgym_ppo.ppo import DiscreteFF
import torch
import rocketsimvis_rlgym_sim_client as rsv

# Load your converted checkpoint
from rlgym_ppo.ppo import DiscreteFF
policy_state = torch.load("C:\\Users\\jepse\\Desktop\\NIGELBOT\\RocketSimVis-main\\rocketsimvis-main\\python_checkpoint\\PPO_POLICY.pt")
policy = DiscreteFF(256, 90, [256, 256, 256, 256, 256, 256], torch.device("cpu"))
policy.load_state_dict(policy_state)
policy.eval()

env = rlgym_sim.make(
    obs_builder=AdvancedObs(),
    action_parser=DefaultAction(),
    terminal_conditions=[TimeoutCondition(300), GoalScoredCondition()]
)

type(env).render = lambda self: rsv.send_state_to_rocketsimvis(self._prev_state)

obs = env.reset()
while True:
    env.render()
    actions, _ = policy.act(obs)
    obs, reward, done, info = env.step(actions)
    if done:
        obs = env.reset()